/*
 * Copyright (C) 2009-2023 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

/*
  This tool replaces a function that may throw an exception (foo), a simple replacement or signature replacement.
  The signature analysis routine is used to print the value passed in IARG_RETURN_IP, which is compared
  with the expected value printed in the IMG instrumentation callback. 
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

#ifdef TARGET_WINDOWS
namespace WND
{
#include <windows.h>
}
#endif

KNOB< string > KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "", "specify file name");
ostream* Out = NULL;

static KNOB< BOOL > KnobReplaceSignature(KNOB_MODE_WRITEONCE, "pintool", "replace_sig", "0",
                                         "Use RTN_ReplaceSignatureProbed instead of RTN_ReplaceProbed");

typedef int (*foo_t)();

static AFUNPTR foo_ptr;

static int foo_rep()
{
    *Out << "foo called" << endl;
    return ((foo_t)foo_ptr)();
}

static int foo_repsig(foo_t orig_foo, ADDRINT returnIp)
{
    *Out << "foo rep called" << endl;

    int res = orig_foo();

    // Will not execute if exception occurs in previous statement
    *Out << "Caller IP = " << hex << returnIp << endl << flush;

    return res;
}

static VOID on_module_loading(IMG img, VOID* data)
{
    if (IMG_IsMainExecutable(img))
    {
        RTN routine = RTN_FindByName(img, "foo");
        if (!RTN_Valid(routine))
        {
            routine = RTN_FindByName(img, "_foo");
        }
        ASSERTX(RTN_Valid(routine));

        if (KnobReplaceSignature)
        {
            PROTO foo_proto = PROTO_Allocate(PIN_PARG(int), CALLINGSTD_DEFAULT, "foo", PIN_PARG_END());
            foo_ptr         = RTN_ReplaceSignatureProbed(routine, (AFUNPTR)foo_repsig, // analysis routine
                                                 IARG_PROTOTYPE, foo_proto,    // prototype
                                                 IARG_ORIG_FUNCPTR,            // address of replaced function
                                                 IARG_RETURN_IP,               // return address in caller
                                                 IARG_END);
        }
        else
        {
            foo_ptr = RTN_ReplaceProbed(routine, (AFUNPTR)(foo_rep));
        }
        ASSERTX(foo_ptr != NULL);

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
