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
 * This pintool replaces foo but not at the entry point to the function.
 * It searches for the beginning of the assembly pattern that forces relocation and probes there.
 * foo() throws an exception that is caught and handles by the application main.
 * The pintool verifies the replacement function did not continue beyond the call to foo since foo threw an exception.
 * The application will verify that the exception propagated properly.
 */

KNOB< string > KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "", "specify file name");
ostream* Out = NULL;

typedef void (*foo_t)();

STATIC AFUNPTR foo_ptr = NULL;

STATIC VOID foo_L0_replacement()
{
    *Out << __FUNCTION__ << " called" << endl;
    return ((foo_t)foo_ptr)();
}

static VOID ImageLoad(IMG img, VOID* data)
{
    if (IMG_IsMainExecutable(img))
    {
        const char* rtnName = "foo";
        RTN rtn             = RTN_FindByName(img, C_MANGLE(rtnName));
        ASSERT(RTN_Valid(rtn), rtnName + " not found");
        *Out << rtnName << " found in address " << hex << RTN_Address(rtn) << endl;

        // Search for the first jmp instruction - this is where the inline assembly pattern begins.
        // We want to place the probe there.
        ADDRINT firstJmpAddress = 0;
        RTN_Open(rtn);
        for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins))
        {
            if (INS_IsDirectBranch(ins))
            {
                firstJmpAddress = INS_Address(ins);
                *Out << "Move probe point to address " << hex << firstJmpAddress << endl;
                break;
            }
        }
        RTN_Close(rtn);

        ASSERTX(firstJmpAddress != 0);
        RTN rtnNew = RTN_CreateAt(firstJmpAddress, "foo_L0");
        ASSERTX(RTN_Valid(rtnNew));

        // Without relocation - expected to fail
        foo_ptr = RTN_ReplaceProbed(rtnNew, (AFUNPTR)foo_L0_replacement);
        ASSERT((foo_ptr == NULL), "foo cannot be probed without relocation");

        // With relocation - expected to succeed
        foo_ptr = RTN_ReplaceProbedEx(rtnNew, PROBE_MODE_ALLOW_RELOCATION, (AFUNPTR)foo_L0_replacement);
        ASSERT((foo_ptr != NULL), "Failed to probe foo with relocation");
        *Out << rtnName << " probed (relocation) in address " << hex << firstJmpAddress << endl;
    }
}

int main(int argc, char** argv)
{
    PIN_InitSymbols();

    if (!PIN_Init(argc, argv))
    {
        Out = KnobOutputFile.Value().empty() ? &cout : new std::ofstream(KnobOutputFile.Value().c_str());
        IMG_AddInstrumentFunction(ImageLoad, 0);
        PIN_StartProgramProbed();
    }

    exit(1);
}
