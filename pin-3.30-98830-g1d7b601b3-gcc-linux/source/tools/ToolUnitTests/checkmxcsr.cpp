/*
 * Copyright (C) 2023-2023 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

/*! @file
 *  This test checks the proper function of IARG_EXPOSE for MXCSR register.
 *  This tool works with checkmxcsr_app.exe application. The application extracts mxcsr register and checks
 *  at the end if the register has changed or not.
 *  The application uses two fnop instructions, fnop is used as a marker in order to insert our
 *  analysis routine at the right places: just before the fnops.
 *  This analysis routine (queryandsetmxcsr() below) is setting bit 0x40 and/or 0x20 of the mxcsr register
 *  which default value is 0x1f80, resulting in value 0x1fc0/0x1fe0.
 *  Since mxcsr register is isolated then the application will not see the register change after the
 *  first fnop. The analysis routine after the second fnop uses IARG_EXPOSE for MXCSR register
 *  which means that its value
 *  should be seen by the application. The application checks - at the end - that the mxcsr value
 *  has changed accordingly.
 */

#include "pin.H"
#include <iostream>
#include <fstream>

using std::cerr;
using std::cout;
using std::endl;
using std::string;

void set_mxcsr(unsigned int);
unsigned int get_mxcsr();

// Counter that tracks the number of fnop seen
unsigned int fnopCount = 0;

/*!
 *  Print out help message.
 */
INT32 Usage()
{
    cerr << "This tool inserts an anlysis routine before fnop instructions." << endl;
    cerr << "this analysis routine changes mxcsr register and sets its value:" << endl;
    cerr << "It sets DMZ bits 0x40 and/or 0x20." << endl;
    cerr << "The corresponding application checks if (after fnop) the mxcsr register" << endl;
    cerr << "has changed or not." << endl;
    return -1;
}

/* ===================================================================== */
// Instrumentation callbacks
/* ===================================================================== */

/*!
 * Print out analysis results.
 * This function is called when the application exits.
 * @param[in]   code            exit code of the application
 * @param[in]   v               value specified by the tool in the 
 *                              PIN_AddFiniFunction function call
 */
VOID Fini(INT32 code, VOID* v)
{
    cout << "===============================================" << endl;
    cout << "Number of fnop: " << fnopCount << endl;
    cout << "===============================================" << endl;
}

VOID someroutine() { printf("analysis(%s): between two analysis routine: mxcsr = 0x%x\n", __FUNCTION__, get_mxcsr()); }

// Analysis routines
VOID queryandsetmxcsr(unsigned int bits)
{
    unsigned int mxcsr                       = get_mxcsr();
    printf("analysis(%s): mxcsr = 0x%x\n", __FUNCTION__, mxcsr);
    printf("analysis(%s): setting bits 0x%x\n", __FUNCTION__, bits);
    // at default mxcsr=0x1f80. We set bits to change it.
    mxcsr |= bits;
    set_mxcsr(mxcsr);
    mxcsr = get_mxcsr();
    printf("analysis(%s): after change: mxcsr = 0x%x\n", __FUNCTION__, mxcsr);
}

// Instrumentation routine
VOID Instruction(INS ins, VOID* v)
{
    const unsigned int MXCSR_DMZ_BIT       = 0x40;
    const unsigned int MXCSR_PRECISION_BIT = 0x20;
    OPCODE opc = INS_Opcode(ins);
    if (opc == XED_ICLASS_FNOP)
    {
        fnopCount++;
        if (1 == fnopCount)
        {
            unsigned int bits = MXCSR_DMZ_BIT;
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)queryandsetmxcsr, IARG_UINT32, bits, IARG_END);
            bits = MXCSR_PRECISION_BIT;
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)queryandsetmxcsr, IARG_UINT32, bits, IARG_END);
        }
        if (2 == fnopCount)
        {
            REGSET reg;
            REGSET_Clear(reg);
            REGSET_Insert(reg, REG_MXCSR);
            unsigned int bits = MXCSR_DMZ_BIT;
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)queryandsetmxcsr, IARG_EXPOSE, &reg, IARG_UINT32, bits, IARG_END);
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)someroutine, IARG_END);
            bits = MXCSR_PRECISION_BIT;
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)queryandsetmxcsr, IARG_EXPOSE, &reg, IARG_UINT32, bits, IARG_END);
        }
    }
}

/*!
 * The main procedure of the tool.
 * This function is called when the application image is loaded but not yet started.
 * @param[in]   argc            total number of elements in the argv array
 * @param[in]   argv            array of command line arguments, 
 *                              including pin -t <toolname> -- ...
 */
int main(int argc, char* argv[])
{
    // Initialize PIN library. Print help message if -h(elp) is specified
    // in the command line or the command line is invalid
    if (PIN_Init(argc, argv))
    {
        return Usage();
    }

    // Register function to be called to instrument instructions
    INS_AddInstrumentFunction(Instruction, 0);

    // Register function to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);

    // Start the program, never returns
    PIN_StartProgram();

    return 0;
}
