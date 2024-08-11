/*
 * Copyright (C) 2009-2023 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string>
#include <stdlib.h>
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

#ifdef TARGET_WINDOWS
namespace WND
{
#include <windows.h>
}
#endif
#include "tool_macros.h"

/*
 * This pintool replaces foo twice (probe on probe), once as plain replacement and once with signature replacement.
 * foo() throws an exception.
 * The pintool verifies the replacement function did not continue beyond the call to foo since foo threw an exception.
 * The application will verify that the exception propagated properly.
 */

// A number that represents the replacement mode (default=1)
// 1: RTN_ReplaceProbed for both
// 2: RTN_ReplaceSignatureProbed for both
// 3: RTN_ReplaceProbed + RTN_ReplaceSignatureProbed
const UINT32 mode_replace_replace       = 1;
const UINT32 mode_replacesig_replacesig = 2;
const UINT32 mode_replace_replacesig    = 3;
static KNOB< UINT32 > KnobReplacementMode(KNOB_MODE_WRITEONCE, "pintool", "repl_mode", "1",
                                          "The replacement mode (RTN_ReplaceProbed/RTN_ReplaceSignatureProbed)");
KNOB< string > KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "", "specify file name");
ostream* Out = NULL;

typedef int (*foo_t)();

static AFUNPTR foo_ptr1 = NULL;
static AFUNPTR foo_ptr2 = NULL;

static int foo_rep1()
{
    *Out << __FUNCTION__ << " called" << endl;
    int ret = ((foo_t)foo_ptr1)();
    // Will not execute if exception occurs in previous statement
    return ret;
}

static int foo_rep2()
{
    *Out << __FUNCTION__ << " called" << endl;
    int ret = ((foo_t)foo_ptr2)();
    // Will not execute if exception occurs in previous statement
    return ret;
}

static int foo_repsig1(foo_t orig_foo, ADDRINT returnIp)
{
    *Out << __FUNCTION__ << " called" << endl;
    int res = orig_foo();

    // Will not execute if exception occurs in previous statement
    *Out << "Caller IP = " << hex << returnIp << endl << flush;
    return res;
}

static int foo_repsig2(foo_t orig_foo, ADDRINT returnIp)
{
    *Out << __FUNCTION__ << " called" << endl;
    int res = orig_foo();

    // Will not execute if exception occurs in previous statement
    *Out << "Caller IP = " << hexstr(returnIp) << endl;

    return res;
}

static VOID on_module_loading(IMG img, VOID* data)
{
    if (IMG_IsMainExecutable(img))
    {
        RTN rtn = RTN_FindByName(img, C_MANGLE("foo"));
        if (!RTN_Valid(rtn))
        {
            rtn = RTN_FindByName(img, C_MANGLE("_foo"));
        }
        ASSERTX(RTN_Valid(rtn));

        if (KnobReplacementMode.Value() == mode_replace_replace)
        {
            foo_ptr1 = RTN_ReplaceProbed(rtn, (AFUNPTR)foo_rep1);
            foo_ptr2 = RTN_ReplaceProbed(rtn, (AFUNPTR)foo_rep2);
        }
        else if (KnobReplacementMode.Value() == mode_replacesig_replacesig)
        {
            PROTO foo_proto = PROTO_Allocate(PIN_PARG(int), CALLINGSTD_DEFAULT, "foo", PIN_PARG_END());
            foo_ptr1        = RTN_ReplaceSignatureProbed(rtn, (AFUNPTR)foo_repsig1, IARG_PROTOTYPE, foo_proto, IARG_ORIG_FUNCPTR,
                                                  IARG_RETURN_IP, IARG_END);
            foo_ptr2        = RTN_ReplaceSignatureProbed(rtn, (AFUNPTR)foo_repsig2, IARG_PROTOTYPE, foo_proto, IARG_ORIG_FUNCPTR,
                                                  IARG_RETURN_IP, IARG_END);
        }
        else if (KnobReplacementMode.Value() == mode_replace_replacesig)
        {
            PROTO foo_proto = PROTO_Allocate(PIN_PARG(int), CALLINGSTD_DEFAULT, "foo", PIN_PARG_END());
            foo_ptr1        = RTN_ReplaceSignatureProbed(rtn, (AFUNPTR)foo_repsig1, IARG_PROTOTYPE, foo_proto, IARG_ORIG_FUNCPTR,
                                                  IARG_RETURN_IP, IARG_END);
            foo_ptr2 = RTN_ReplaceProbed(rtn, (AFUNPTR)foo_rep2);
        }
        else
        {
            ASSERT(FALSE, "Illegal replace mode " + decstr(KnobReplacementMode.Value()) + "\n");
        }
        ASSERTX(foo_ptr1 != NULL);
        ASSERTX(foo_ptr2 != NULL);
    }
}

int main(int argc, char** argv)
{
    PIN_InitSymbols();

    if (!PIN_Init(argc, argv))
    {
        Out = KnobOutputFile.Value().empty() ? &cout : new std::ofstream(KnobOutputFile.Value().c_str());
        IMG_AddInstrumentFunction(on_module_loading, 0);
        PIN_StartProgramProbed();
    }

    exit(1);
}
