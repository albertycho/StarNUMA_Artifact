/*
 * Copyright (C) 2009-2023 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

/*
  This tool instruments a function, foo(), that may throw an exception depending on a global variable, with a BEFORE and AFTER.
  The BEFORE analysis routine is used to calculate and print the expected return value of foo.
  The AFTER analysis routine is used to print the value passed in IARG_RETURN_IP, which is compared
  with the expected value printed in the IMG instrumentation callback. 
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

#if defined(TARGET_LINUX) || defined(TARGET_MAC)
#define CALL_TYPE CALLINGSTD_DEFAULT
#else
#if defined(TARGET_IA32E)
#define CALL_TYPE CALLINGSTD_DEFAULT
#else
#define CALL_TYPE CALLINGSTD_REGPARMS
#endif
#endif

KNOB< string > KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "", "specify file name");
ostream* Out = NULL;

static VOID foo_before(int a0, int a1, int a2, int a3, int a4, int a5, int a6)
{
    *Out << __FUNCTION__ << " called" << endl << flush;
    *Out << "In pintool, result=" << dec << (a0 + a1 + a2 + a3 + a4 + a5 + a6) << endl << flush;
}

static VOID foo_after(ADDRINT returnIp)
{
    // Will not execute if exception occurs in foo
    *Out << __FUNCTION__ << " called" << endl << flush;
    *Out << "Caller IP = " << hex << returnIp << endl << flush;
}

static VOID on_module_loading(IMG img, VOID* data)
{
    if (IMG_IsMainExecutable(img))
    {
        RTN routine = RTN_FindByName(img, C_MANGLE("foo"));
        if (!RTN_Valid(routine))
        {
            routine = RTN_FindByName(img, C_MANGLE("@foo@28"));
        }
        ASSERTX(RTN_Valid(routine));
        ASSERTX(RTN_IsSafeForProbedInsertion(routine));

        PROTO foo_proto = PROTO_Allocate(PIN_PARG(int), CALL_TYPE, "foo", PIN_PARG(int), PIN_PARG(int), PIN_PARG(int),
                                         PIN_PARG(int), PIN_PARG(int), PIN_PARG(int), PIN_PARG(int), PIN_PARG_END());

        ASSERTX(RTN_InsertCallProbed(
            routine, IPOINT_BEFORE, AFUNPTR(foo_before), IARG_PROTOTYPE, foo_proto, IARG_FUNCARG_ENTRYPOINT_VALUE, 6,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 5, IARG_FUNCARG_ENTRYPOINT_VALUE, 4, IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, IARG_FUNCARG_ENTRYPOINT_VALUE, 1, IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_END));
        ASSERTX(
            RTN_InsertCallProbed(routine, IPOINT_AFTER, AFUNPTR(foo_after), IARG_PROTOTYPE, foo_proto, IARG_RETURN_IP, IARG_END));

        // Print all the addresses of instructions after a direct call to foo().
        ADDRINT fooAddress = RTN_Address(routine);
        for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec))
        {
            for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn))
            {
                RTN_Open(rtn);
                INS ins = RTN_InsHead(rtn);
                while (INS_Valid(ins))
                {
                    if (INS_IsCall(ins) && INS_IsDirectControlFlow(ins) && INS_DirectControlFlowTargetAddress(ins) == fooAddress)
                    {
                        ins = INS_Next(ins);
                        if (INS_Valid(ins)) *Out << "Return from call to " << hex << INS_Address(ins) << endl;
                    }
                    else
                    {
                        ins = INS_Next(ins);
                    }
                }
                RTN_Close(rtn);
            }
        }
    }
}

int main(int argc, char** argv)
{
    PIN_InitSymbols();

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
