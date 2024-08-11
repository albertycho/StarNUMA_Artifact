/*
 * Copyright (C) 2023-2023 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

/*
 * This tool is testing the fallback mechanism for instrumenting probes, using Insert, Replace or ReplaceSignature.
 *
 * Pintool mode of operation:
 *  The pintool expects 3 functions which name begins with "foo" and can be instrumented in all probe sizes.
 *  Each function is called once.
 *  The functions are instrumented according to the mode.
 *  In addition, the pintool defines environment variables to artificially limit the number of allocations in the different memory pools.
 *  This will activate the fallback mechanism.
 *  Success criteria:
 *      o   All the functions were successfully instrumented.
 *      o   The analysis routines were called the expected number of times.
 */

#include "pin.H"
#include <iostream>
#include <stdlib.h>
#include "tool_macros.h"

// 1 RTN_InsertCallProbed
// 2 RTN_ReplaceProbed
// 3 RTN_ReplaceSignatureProbed
KNOB< UINT32 > KnobMode(KNOB_MODE_WRITEONCE, "pintool", "mode", "1",
                        "1=RTN_InsertCallProbed (default), 2=RTN_ReplaceProbed, 3=RTN_ReplaceSignatureProbed");

// Two knobs that specify the limit for successful allocations.
// The value 0 (default) specifies there are no limits.
// Other values represent the number of times the allocation should succeed before starting to fail.
KNOB< UINT32 > KnobLimit2GB(KNOB_MODE_WRITEONCE, "pintool", "limit2gb", "0",
                            "Number of allocations in the near 2GB after which allocations should fail");
KNOB< UINT32 > KnobLimit32bit(KNOB_MODE_WRITEONCE, "pintool", "limit32bit", "0",
                              "Number of allocations in the 32 bit memory space after which allocations should fail");

// The number of times we expect to visit all analysis routines during the run
static UINT32 expected_analysis_routine_visits = 0;

typedef VOID (*FUNCPTR_FOO)(int*);

static void foo_sigreplacement(ADDRINT rtnName, FUNCPTR_FOO origFuncptr, ADDRINT arg0)
{
    expected_analysis_routine_visits--;
    std::cout << __FUNCTION__ << std::endl;
    std::cout << "Remaining visit count " << std::dec << expected_analysis_routine_visits << std::endl;
    std::cout << "Replacement for " << (CHAR*)rtnName << "()" << std::endl;
    origFuncptr((int*)arg0);
}

static void foo_replacement(int* counter)
{
    expected_analysis_routine_visits--;
    std::cout << __FUNCTION__ << std::endl;
    std::cout << "Remaining visit count " << std::dec << expected_analysis_routine_visits << std::endl;
}

VOID before_foo(ADDRINT rtnName)
{
    expected_analysis_routine_visits--;
    std::cout << __FUNCTION__ << std::endl;
    std::cout << "Remaining visit count " << std::dec << expected_analysis_routine_visits << std::endl;
    std::cout << "BEFORE " << (CHAR*)rtnName << std::endl;
}

VOID ImageLoad(IMG img, VOID* v)
{
    if (!IMG_IsMainExecutable(img)) return;

    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec))
    {
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn))
        {
            if (strncmp(RTN_Name(rtn).c_str(), "foo", strlen("foo")) == 0) // function name start with foo
            {
                switch (KnobMode)
                {
                    case 1:
                    {
                        std::cout << "RTN_InsertCallProbed(" << RTN_Name(rtn) << ")" << std::endl;
                        BOOL instrumented = RTN_InsertCallProbed(rtn, IPOINT_BEFORE, AFUNPTR(before_foo), IARG_ADDRINT,
                                                                 strdup(RTN_Name(rtn).c_str()), IARG_END);
                        ASSERTX(instrumented);
                        expected_analysis_routine_visits++;
                    }
                    break;

                    case 2:
                    {
                        std::cout << "RTN_ReplaceSignatureProbed " << RTN_Name(rtn) << " ( 0x" << std::hex << RTN_Address(rtn)
                                  << " ) " << std::endl;
                        AFUNPTR addr = RTN_ReplaceProbed(rtn, AFUNPTR(foo_replacement));
                        ASSERTX(addr != NULL);
                        expected_analysis_routine_visits++;
                    }
                    break;

                    case 3:
                    {
                        std::cout << "RTN_ReplaceSignatureProbed " << RTN_Name(rtn) << " ( 0x" << std::hex << RTN_Address(rtn)
                                  << " ) " << std::endl;
                        PROTO proto_foo =
                            PROTO_Allocate(PIN_PARG(void*), CALLINGSTD_DEFAULT, "foo", PIN_PARG(int*), PIN_PARG_END());
                        AFUNPTR addr = RTN_ReplaceSignatureProbed(rtn, AFUNPTR(foo_sigreplacement), IARG_PROTOTYPE, proto_foo,
                                                                  IARG_ADDRINT, strdup(RTN_Name(rtn).c_str()), IARG_ORIG_FUNCPTR,
                                                                  IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_END);
                        ASSERTX(addr != NULL);
                        expected_analysis_routine_visits++;
                    }
                    break;
                }
            }
        }
    }
    ASSERTX(expected_analysis_routine_visits >= 3);
}

int main(INT32 argc, CHAR* argv[])
{
    PIN_InitSymbols();
    PIN_Init(argc, argv);

    // Set environment variables to be read by Pin that will artifically limit
    // the number of allocations in the different memory pools.
    UINT32 limit2GB   = KnobLimit2GB.Value();
    UINT32 limit32bit = KnobLimit32bit.Value();
    if (limit2GB != 0)
    {
        setenv("PROBE_ALLOC_2GB", std::to_string(limit2GB).c_str(), 1);
    }
    if (limit32bit != 0)
    {
        setenv("PROBE_ALLOC_32BIT", std::to_string(limit32bit).c_str(), 1);
    }

    IMG_AddInstrumentFunction(ImageLoad, 0);
    PIN_StartProgramProbed();
    return 0;
}
