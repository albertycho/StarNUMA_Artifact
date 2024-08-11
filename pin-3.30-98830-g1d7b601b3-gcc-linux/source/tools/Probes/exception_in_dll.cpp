/*
 * Copyright (C) 2023-2023 Intel Corporation.
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
using std::hex;
using std::ios;
using std::ostream;
using std::string;

/*
 * This pintool replaces functions that throw excpetions (foo1 and ExcInDll).
 * ExcInDll resides in a separate library (not the main executable).
 * It also instruments BEFORE and AFTER to foo2 that calls ExcInDll.
 * The application will verify that exception was delivered properly.
 * Also we will verify that the foo2 AFTER wasn't called since there was an exception.
 */
#ifdef TARGET_WINDOWS
namespace WND
{
#include <windows.h>
}
#endif
#include "tool_macros.h"

KNOB< string > KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "", "specify file name");
ostream* Out = NULL;

typedef void (*foo_t)();

static void foo1_rep(foo_t orig_foo1)
{
    *Out << "foo1 rep called" << endl;
    orig_foo1();
}

static void ExcInDll_rep(foo_t orig_func)
{
    *Out << "ExcInDll rep called" << endl;
    orig_func();
}

static VOID foo2_before() { *Out << "foo2 before called" << endl; }
static VOID foo2_after() { *Out << "foo2 after called" << endl; }

static VOID on_module_loading(IMG img, VOID* data)
{
    if (IMG_IsMainExecutable(img))
    {
        // Replace foo1
        RTN rtn = RTN_FindByName(img, C_MANGLE("foo1"));
        ASSERTX(RTN_Valid(rtn));
        ASSERTX(RTN_IsSafeForProbedReplacement(rtn));
        PROTO foo1_proto = PROTO_Allocate(PIN_PARG(void), CALLINGSTD_DEFAULT, "foo1", PIN_PARG_END());
        AFUNPTR foo1_ptr =
            RTN_ReplaceSignatureProbed(rtn, (AFUNPTR)foo1_rep, IARG_PROTOTYPE, foo1_proto, IARG_ORIG_FUNCPTR, IARG_END);
        ASSERTX(foo1_ptr != 0);

        // Instrument foo2 BEFORE and AFTER
        rtn = RTN_FindByName(img, C_MANGLE("foo2"));
        ASSERTX(RTN_Valid(rtn));
        ASSERTX(RTN_IsSafeForProbedInsertion(rtn));
        PROTO foo2_proto = PROTO_Allocate(PIN_PARG(void), CALLINGSTD_DEFAULT, "foo2", PIN_PARG_END());
        ASSERTX(RTN_InsertCallProbed(rtn, IPOINT_BEFORE, AFUNPTR(foo2_before), IARG_PROTOTYPE, foo2_proto, IARG_END));
        ASSERTX(RTN_InsertCallProbed(rtn, IPOINT_AFTER, AFUNPTR(foo2_after), IARG_PROTOTYPE, foo2_proto, IARG_END));
    }
    else
    {
        // Replace ExcInDll
        RTN rtn = RTN_FindByName(img, C_MANGLE("ExcInDll"));
        if (RTN_Valid(rtn))
        {
            ASSERTX(RTN_IsSafeForProbedReplacement(rtn));
            PROTO ExcInDll_proto = PROTO_Allocate(PIN_PARG(void), CALLINGSTD_DEFAULT, "ExcInDll", PIN_PARG_END());
            AFUNPTR ExcInDll_ptr = RTN_ReplaceSignatureProbed(rtn, (AFUNPTR)ExcInDll_rep, IARG_PROTOTYPE, ExcInDll_proto,
                                                              IARG_ORIG_FUNCPTR, IARG_END);
            ASSERTX(ExcInDll_ptr != 0);
        }
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
