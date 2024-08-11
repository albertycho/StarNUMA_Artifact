/*
 * Copyright (C) 2020-2021 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

/*! @file
 * This tool verifies that IARG_MEMORYOP_EA of XLAT instruction has
 * the same value both when the XLAT has 0x67 prefix ('addr32 xlat') and when it doesn't.
 * Note - We test this only on 64 bit and not in 32 bit since on 32 bit, 16 byes address size prefix ('addr16 xlat')
 * can't be tested since it means accessing memory lower than 0x10000. We can't allocate address in this memory space.
 */
#include "pin.H"
#include <iostream>

#define INVALID_EA ~static_cast< ADDRINT >(0)

static BOOL xlat_analyzed_twice = FALSE;
static ADDRINT last_xlat_ea     = INVALID_EA;

static VOID xlat_ins_analysis(ADDRINT ea, ADDRINT ea2)
{
    if ((ea != ea2))
    {
        // Making sure register parameters are not overridden due to implementation of EA transfer to analysis routine (BUG 5161).
        std::cerr << "EA: 0x" << std::hex << ea << ", EA2: 0x" << ea2 << std::endl;
        PIN_ExitProcess(1);
    }

    if (last_xlat_ea == INVALID_EA) // first time analysis routine is called
    {
        last_xlat_ea = ea;
        return;
    }
    xlat_analyzed_twice = TRUE;
    if (ea != last_xlat_ea)
    {
        std::cerr << "XLAT IARG_MEMORYOP_EA values differ: 0x" << std::hex << last_xlat_ea << ", 0x" << ea << std::endl;
        PIN_ExitProcess(1);
    }
}

VOID Image(IMG img, VOID* v)
{
    if (!IMG_IsMainExecutable(img)) return;

    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec))
    {
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn))
        {
            RTN_Open(rtn);
            for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins))
            {
                if (INS_Opcode(ins) == XED_ICLASS_XLAT)
                {
                    for (UINT32 memOp = 0; memOp < INS_MemoryOperandCount(ins); memOp++)
                    {
                        if (INS_MemoryOperandIsRead(ins, memOp))
                        {
                            INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(xlat_ins_analysis), IARG_MEMORYOP_EA, memOp,
                                           IARG_MEMORYOP_EA, memOp, IARG_END);
                        }
                    }
                }
            }
            RTN_Close(rtn);
        }
    }
}

VOID Fini(INT32 code, VOID* v)
{
    if (!xlat_analyzed_twice)
    {
        std::cerr << "Did not analyze any XLAT instructions" << std::endl;
        PIN_ExitProcess(1);
    }
}

int main(int argc, char* argv[])
{
    PIN_InitSymbols();
    if (PIN_Init(argc, argv))
    {
        std::cerr << "PIN_Init failed" << std::endl;
        return 1;
    }

    IMG_AddInstrumentFunction(Image, 0);
    PIN_AddFiniFunction(Fini, 0);
    PIN_StartProgram();
    return 0;
}
