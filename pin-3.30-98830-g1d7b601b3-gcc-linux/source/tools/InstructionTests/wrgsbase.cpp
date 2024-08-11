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
    cerr << "This tool checks GS/FS segment base register values are correct after they changed by the application.\n";

    cerr << KNOB_BASE::StringKnobSummary();

    cerr << endl;

    return -1;
}

std::string seg_reg_str(REG seg_reg)
{
    switch (seg_reg)
    {
        case LEVEL_BASE::REG_SEG_GS_BASE:
            return "GS";

        case LEVEL_BASE::REG_SEG_FS_BASE:
            return "FS";

        default:
            return "";
    }
}

// Passing the segment base register to analysis routine in two ways (just to cover them both):
// (1) UINT32 seg_reg + (2) ADDRINT seg_base (using context)
VOID handle_segment_register(UINT32 seg_reg, CONTEXT const* context, ADDRINT seg_base, std::string* disassm)
{
    ADDRINT seg_base_from_context = PIN_GetContextReg(context, (REG)seg_reg);

    if (seg_base != seg_base_from_context)
    {
        // different segment base addresses (from context and from IARG_REG_VALUE, value should be equal)
        cerr << "[TOOL] Error: different segment base address values:" << endl;
        cerr << "[TOOL] Error: base address from IARG_CONST_CONTEXT = 0x" << std::hex << seg_base << endl;
        cerr << "[TOOL] Error: base address from IARG_REG_VALUE     = 0x" << std::hex << seg_base_from_context << endl;
        ASSERTX(FALSE);
    }
    else if ((seg_base == 0) || (seg_base_from_context == 0))
    {
        // check that both segment base address are not 0 (since application directly writes GS/FS base address)
        cerr << endl
             << "[TOOL] Error: seg_from_context = 0x" << std::hex << seg_base_from_context << " seg_base 0x" << std::hex
             << seg_base << ", ins disasm: " << *disassm << endl;
        ASSERTX(FALSE);
    }
    else
    {
        cout << "\n[TOOL] " << seg_reg_str((REG)seg_reg) << " segment base address = 0x" << std::hex << seg_base_from_context
             << " , ins disasm: " << *disassm << endl;
    }

    return;
}

VOID InstrumentWrGsBase(INS ins, VOID* v)
{
    std::string str   = INS_Disassemble(ins);
    std::string* diss = new std::string(str);

    // Passing the segment base register to analysis routine in two ways (just to cover them both):
    // (1) UINT32 seg_reg + (2) ADDRINT seg_base (using context)
    INS_InsertCall(ins, IPOINT_AFTER, (AFUNPTR)handle_segment_register, IARG_UINT32, REG_SEG_GS_BASE, IARG_CONST_CONTEXT,
                   IARG_REG_VALUE, REG_SEG_GS_BASE, IARG_PTR, diss, IARG_END);
}

VOID ImageLoad(IMG img, VOID* v)
{
    if (!IMG_IsMainExecutable(img))
    {
        return;
    }

    // Iterate all instructions in application and instrument only 'wrgsbase' instruction
    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec))
    {
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn))
        {
            RTN_Open(rtn);
            for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins))
            {
                if (INS_Opcode(ins) == XED_ICLASS_WRGSBASE)
                {
                    InstrumentWrGsBase(ins, NULL);
                }
            }
            RTN_Close(rtn);
        }
    }
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char* argv[])
{
    if (PIN_Init(argc, argv))
    {
        return Usage();
    }

    IMG_AddInstrumentFunction(ImageLoad, 0);

    // Never returns
    PIN_StartProgram();

    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
