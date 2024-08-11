/*
 * Copyright (C) 2023-2023 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

/* ===================================================================== */
/*! @file
 * This pintool replaces a routine (RtlLeaveCriticalSection) that causes exception in probed bytes.
 * In addition it generates exceptions inside the pintool (access violation and stack overflow) and catches them.
 * The test validates that exceptions thrown from the application (from LeaveCriticalSection) are passed properly to the application.
 */

#include <stdio.h>
#include <string>
#include <fstream>
#include <iostream>
#include "pin.H"
using std::cerr;
using std::cout;
using std::dec;
using std::endl;
using std::flush;
using std::hex;
using std::ios;
using std::ostream;
using std::string;

namespace WND
{
#include <windows.h>
}

KNOB< string > KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "", "specify file name");
ostream* Out = NULL;

static KNOB< BOOL > KnobReplaceSignature(KNOB_MODE_WRITEONCE, "pintool", "replace_sig", "0",
                                         "Use RTN_ReplaceSignatureProbed instead of RTN_ReplaceProbed");

// Signature of RtlLeaveCriticalSection
typedef VOID(WINAPI* rtl_leave_critical_section_call_t)(__inout WND::LPCRITICAL_SECTION lpCriticalSection);

// Address of original RtlLeaveCriticalSection
void (*g_RtlLeaveCriticalSection_ptr)(void);

/*
 * This original function is all the time being optimized by clang
 * which goes around the stack violation. Therefore the following
 * coding which enforces clang to generate code that violates
 * stack access
 */
#pragma optimize("", off)
__declspec(noinline) int GenerateStackOverflow()
{
    // The stack overflow exception works in the following way:
    // 1. we define an array with a huge size, in this case: 1GB
    // 2. The size is not really important but it should be bigger than
    //    the stack size.
    // 3. the code accesses this array but starting with the highest
    //    addresses, i.e. arr[0x40000000-1] and going down.
    // 4. iteration after iteration the lower addresses of the stack will
    //    be accessed until we reach the bottom and beyond: this is
    //    the stack overflow exception.
    // 5. Although... the compiler inserts a prolog call to _chkstck()
    //    this method receives as parameter via ecx the size of the frame
    //    _chkstck() does exactly the same as written below to finally
    //    cause the stack overflow exception. So the code below is to
    //    make sure that _chkstck() get the right parameter and does
    //    the work
    // pragma optimize off is used to make sure a compiler such as
    // clang will not remove that function which does basically nothing
    uint8_t *current, arr[0x40000000];
    for (current = arr + sizeof(arr) - 1; current > arr; current -= PAGESIZE)
    {
        // write any value on stack
        *current = 0x30;
    }

    return 0;
}
#pragma optimize("", on)

int* g_ptr;

/*
 * Same with access violation: clang detects and generates ud
 * to go around we need to hide the NULL ptr value
 */
void do_access_violation()
{
    *g_ptr = 17;
}

STATIC VOID GenerateAndCatchExceptions()
{
    // Generate an access violation exception and catch it
    __try
    {
        do_access_violation();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        *Out << "Exception in RtlLeaveCriticalSection replacement routine" << endl << flush;
    }

    // Generate a stack overflow exception and catch it
    __try
    {
        volatile int i = GenerateStackOverflow();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        if (_resetstkoflw())
        {
            *Out << "Stack-Overflow in RtlLeaveCriticalSection replacement routine" << endl << flush;
            fflush(stdout);
        }
    }
}

VOID WINAPI RtlLeaveCriticalSection_rep(__inout WND::LPCRITICAL_SECTION lpCriticalSection)
{
    if (lpCriticalSection == NULL)
    {
        GenerateAndCatchExceptions();
    }
    // Call the original fuction (Will also generate an exception)
    return (*(rtl_leave_critical_section_call_t)g_RtlLeaveCriticalSection_ptr)(lpCriticalSection);
}

VOID WINAPI RtlLeaveCriticalSection_repsig(rtl_leave_critical_section_call_t orig_RtlLeaveCriticalSection,
                                           WND::LPCRITICAL_SECTION lpCriticalSection, ADDRINT returnIp)
{
    if (lpCriticalSection == NULL)
    {
        GenerateAndCatchExceptions();

        orig_RtlLeaveCriticalSection(lpCriticalSection);

        // Will not execute if exception occurs in previous statement
        *Out << "Caller IP = " << hex << returnIp << endl << flush;
    }
    else
    {
        orig_RtlLeaveCriticalSection(lpCriticalSection);
    }
}

static const char* extract_mod_name_with_ext(const char* full)
{
    const char* slash = NULL;
    char* module_name = NULL;

    slash = strrchr(full, '\\');

    if (slash)
    {
        module_name = _strlwr(_strdup(++slash));
    }
    else
    {
        module_name = _strlwr(_strdup(full));
    }

    return module_name;
}

static VOID instrument_module(IMG img, VOID* data)
{
    const char* module_name = extract_mod_name_with_ext(IMG_Name(img).c_str());

    if (strcmp(module_name, "ntdll.dll") == 0)
    {
        RTN routine = RTN_FindByName(img, "RtlLeaveCriticalSection");
        ASSERTX(RTN_Valid(routine));
        if (KnobReplaceSignature)
        {
            PROTO leave_proto =
                PROTO_Allocate(PIN_PARG(void), CALLINGSTD_STDCALL, "RtlLeaveCriticalSection", PIN_PARG(void*), PIN_PARG_END());
            g_RtlLeaveCriticalSection_ptr =
                RTN_ReplaceSignatureProbed(routine, (AFUNPTR)(RtlLeaveCriticalSection_repsig), // analysis routine
                                           IARG_PROTOTYPE, leave_proto,                        // prototype
                                           IARG_ORIG_FUNCPTR,                                  // address of replaced function
                                           IARG_FUNCARG_ENTRYPOINT_VALUE, 0,                   // arg 0
                                           IARG_RETURN_IP,                                     // return address in caller
                                           IARG_END);
        }
        else
        {
            g_RtlLeaveCriticalSection_ptr = RTN_ReplaceProbed(routine, (AFUNPTR)(RtlLeaveCriticalSection_rep));
        }
        ASSERTX(g_RtlLeaveCriticalSection_ptr != NULL);
        free((void*)(module_name));
    }
}

static VOID on_module_loading(IMG img, VOID* data)
{
    unsigned long origAttrs = 0;

    if (IMG_Valid(img))
    {
        if (!IMG_IsMainExecutable(img))
        {
            instrument_module(img, data);
        }
    }
}

int main(int argc, char** argv)
{
    PIN_InitSymbolsAlt(EXPORT_SYMBOLS);
    g_ptr = NULL;

    if (!PIN_Init(argc, argv))
    {
        Out = KnobOutputFile.Value().empty() ? &cout : new std::ofstream(KnobOutputFile.Value().c_str());
        *Out << hex;
        Out->setf(ios::showbase);
        IMG_AddInstrumentFunction(on_module_loading, 0);
        PIN_StartProgramProbed();
    }

    exit(1);
}
