/*
 * Copyright (C) 2023-2023 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

// This tool verifies the tracking of GS/FS segment base register by pin, after the
// application has directly change it's value by using WRGSBASE/WRFSBASE instructions.

#include "pin.H"
#include <string>
#include <iostream>
using std::cerr;
using std::cout;
using std::endl;

INT32 Usage()
{
    cerr << "This tool verifies error when tool asks for GS segment base register in Windows 32-bit.\n";

    cerr << KNOB_BASE::StringKnobSummary();

    cerr << endl;

    return -1;
}

VOID RequestGsBaseRegister(ADDRINT gs_base)
{
    cout << "[TOOL] GS base address = " << gs_base << endl;
    return;
}

VOID Instruction(INS ins, VOID* v)
{
    // Passing the segment base register to analysis routine
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)RequestGsBaseRegister, IARG_REG_VALUE, REG_SEG_GS_BASE, IARG_END);
}

int main(int argc, char* argv[])
{
    if (PIN_Init(argc, argv))
    {
        return Usage();
    }

    INS_AddInstrumentFunction(Instruction, 0);

    // Never returns
    PIN_StartProgram();

    return 0;
}
