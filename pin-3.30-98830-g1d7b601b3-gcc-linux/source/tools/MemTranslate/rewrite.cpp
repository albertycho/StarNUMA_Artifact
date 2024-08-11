/*
 * Copyright (C) 2023-2023 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

/*
 * Rewrite all memory operands, though we don't actually change the addresses they access.
 * . Test hooks memory write instruction with one memory operand and rep prefix
 * . The memory operand is rerouted to buffer below using temporary g0 register
 * . We add an analysis routine to reroute what written to buffer instead to the original
 *   address such that everything should work the same as before
 * remarks: 
 * . since buffer is global, this test works only with one thread: does not work on Windows
 */
#include <fstream>
#include <iostream>
#include <iomanip>

#include <string.h>
#include "pin.H"
using std::dec;
using std::endl;
using std::hex;
using std::ofstream;
using std::string;

static UINT64 buffer[16];
static ADDRINT memea = 0;
static BOOL fnopFound = FALSE;

// This method checks if we are instrumenting the right instruction
// i.e it is located in the right *_buff_* method, it is located in
// the main executable
static BOOL doTranslate(INS ins)
{
    // We use a fnop instruction just before the mov instruction to rewrite
    // When the fnop instruction is met, fnopFound flag is raised and next
    // mov instruction is the one to instrument for rewrite. See comment
    // in rewrite_app.c, mov_to_buff_32() method.
    OPCODE opc = INS_Opcode(ins);
    if (opc == XED_ICLASS_FNOP)
    {
        fnopFound = TRUE;
        return FALSE;
    }

    int memops = INS_MemoryOperandCount(ins);

    if (memops != 2 && memops != 1)
        return FALSE;

    RTN rtn = INS_Rtn(ins);
    if (!RTN_Valid(rtn))
        return FALSE;

    std::string name = RTN_Name(rtn);
    if (name.find("_buff_") == std::string::npos)
        return FALSE;

    // rep prefix identifies rep stos and rep movs tests
    // fnopFound identifies mov test
    if ((!fnopFound) && (!INS_HasRealRep(ins)))
        return FALSE;

    fnopFound = FALSE;
    IMG img = SEC_Img(RTN_Sec(INS_Rtn(ins)));
    if (IMG_IsMainExecutable(img))
        return TRUE;

    return FALSE;
}

static ADDRINT set_reg(ADDRINT addr)
{
    memea = addr;
    return (VoidStar2Addrint(buffer));
}

static VOID copy_value(UINT32 size)
{
    switch(size) {
        case 1: {
            unsigned char* ptr = (unsigned char*)memea;
            unsigned char* in = (unsigned char*)buffer;
            *ptr = *in;
            break;
        }

        case 2: {
            unsigned short* ptr = (unsigned short*)memea;
            unsigned short* in = (unsigned short*)buffer;
            *ptr = *in;
            break;
        }
            
        case 4: {
            unsigned int* ptr = (unsigned int*)memea;
            unsigned int* in = (unsigned int*)buffer;
            *ptr = *in;
            break;
        }
            
        case 8: {
            unsigned long long* ptr = (unsigned long long*)memea;
            unsigned long long* in = (unsigned long long*)buffer;
            *ptr = *in;
            break;
        }

        default: {
            fprintf(stderr, "Unsupported memory size: %d\n", size);
            PIN_ExitApplication(1);
        }
    }
}

static VOID RewriteIns(INS ins)
{
    /* Rewrite all the memory operands */

    // stos instruction has one memory operand
    // movs has two memory operand.
    // the rewritten destination operand is operand 0
    if (doTranslate(ins))
    {
        fprintf(stderr, "IP: %p ins: %s\n", (void*)INS_Address(ins), INS_Disassemble(ins).c_str());

        // rewrite memop 0 to REG_INST_G0
        INS_RewriteMemoryOperand(ins, 0, REG_INST_G0);

        // Put buffer in REG_INST_G0 and remember original address
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)set_reg, IARG_MEMORYOP_EA, 0, IARG_RETURN_REGS,
                       REG_INST_G0, IARG_END);

        INS_InsertCall(ins, IPOINT_AFTER, (AFUNPTR)copy_value, IARG_MEMORYOP_SIZE, 0, IARG_END);
    }
}

VOID Trace(TRACE trace, VOID* v)
{
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins))
        {
            RewriteIns(ins);
        }
    }
}

int main(int argc, char* argv[])
{
    PIN_Init(argc, argv);
    PIN_InitSymbols();

    TRACE_AddInstrumentFunction(Trace, 0);

    // Never returns
    PIN_StartProgram();

    return 0;
}
