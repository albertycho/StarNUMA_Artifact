/*
 * Copyright (C) 2023-2023 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

/*
 * This test check the correctness of IARG_CHECK_INLINE, which is used to mark analysis routine that is expected to be inlined.
 * We also test -assert_check_inline option which asserts in case specific analysis routine marked with IARG_CHECK_INLINE is not inlined by Pin
 *
 * In this test we instrument non-inlinable analysis routine marked with IARG_CHECK_INLINE :
 * (1) without -assert_check_inline - expects hint in log file
 * (2) with -assert_check_inline    - expects Pin assert and exit
 */

#include "pin.H"
#include <iostream>
#include <fstream>
using std::cerr;
using std::cout;
using std::endl;
using std::string;

UINT32 res = 0;

#if defined(TARGET_LINUX) && defined(TARGET_IA32E) && !defined(__INTEL_COMPILER)
// prevents compiler optimizations which breaks inlining
#pragma GCC push_options
#pragma GCC target("no-sse2")
#endif

VOID NonInlineableFunction(UINT32 one, UINT32 two)
{
    // The evaluation of the expression 'cond1 && cond2' generates jump instruction which breaks the inlining.
    // If 'cond1' is false, then 'cond2' is not being checked.The code simply jumps over the check of 'cond2'.
    // The generation of this jump makes the function non-inlineable.
    res = one && two;
}

#if defined(TARGET_LINUX) && defined(TARGET_IA32E) && !defined(__INTEL_COMPILER)
#pragma GCC pop_options
#endif

INT32 Usage()
{
    cerr << "This test checks the correctness of IARG_CHECK_INLINE usage on analysis routine." << endl
         << "IARG_CHECK_INLINE marks analysis routine as expected by the tool to be inlined by Pin" << endl;
    return -1;
}

VOID Fini(INT32 code, VOID* v)
{
    cout << "Test finished successfully." << endl;
}

VOID Instruction(INS ins, VOID* v)
{
    UINT32 one = 1;
    UINT32 two = 2;

    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)NonInlineableFunction, IARG_UINT32, one, IARG_UINT32, two, IARG_CHECK_INLINE,
                   IARG_END);
}

int main(int argc, char* argv[])
{
    if (PIN_Init(argc, argv))
    {
        return Usage();
    }

    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    // Never returns
    PIN_StartProgram();

    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
