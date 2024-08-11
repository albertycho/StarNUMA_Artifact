/*
 * Copyright (C) 2022-2022 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

#include "pin.H"
#include <iostream>

PIN_LOCK pinLock;

/*
 * This pintool is searching for a function named foo() in an application and instruments it.
 * The instrumentation is done in order to make sure we have visited foo().
 * The tool is used in a test that verifies that the foo application was launched successfully.
 */
VOID BeforeFoo()
{
    PIN_GetLock(&pinLock, PIN_GetTid());
    std::cout << "Before foo" << std::endl;
    PIN_ReleaseLock(&pinLock);
}

VOID ImageLoad(IMG img, VOID* v)
{
    const CHAR* fname = "foo";
    RTN rtn           = RTN_FindByName(img, "foo");
    if (RTN_Valid(rtn))
    {
        if (!RTN_InsertCallProbed(rtn, IPOINT_BEFORE, (AFUNPTR)BeforeFoo, IARG_END))
        {
            std::cerr << "Function \"" << fname << "\" cannot be probed in " << IMG_Name(img) << std::endl;
            PIN_ExitApplication(1);
        }
    }
}

int main(int argc, CHAR* argv[])
{
    PIN_InitSymbols();
    if (PIN_Init(argc, argv))
    {
        std::cerr << "PIN_Init() failed" << std::endl;
        return 1;
    }
    PIN_InitLock(&pinLock);
    IMG_AddInstrumentFunction(ImageLoad, 0);
    PIN_StartProgramProbed();
    return 0;
}
