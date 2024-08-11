/*
 * Copyright (C) 2016-2021 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

#include <iostream>

#include "pin.H"
using std::cerr;
using std::endl;

static KNOB< BOOL > KnobCreateOverlap(KNOB_MODE_WRITEONCE, "pintool", "create_overlap", "0",
                                      "Create routine that overlaps the original and probe it");

VOID before(void) {}

VOID ImageLoad(IMG img, VOID* v)
{
    RTN rtn = RTN_FindByName(img, "foo");

    if (RTN_Valid(rtn))
    {
        if (!KnobCreateOverlap)
        {
            // Make sure the original function is successfully probed
            if (RTN_InsertCallProbed(rtn, IPOINT_BEFORE, AFUNPTR(before), IARG_END) == FALSE)
            {
                cerr << "Failed to probe " << RTN_Name(rtn) << endl;
                PIN_ExitProcess(1);
            }
        }
        else
        {
            // Insert a BEFORE call at the second instruction of foo().
            // Then check probe safety for inserting a BEFORE call at foo().
            // We expect to fail due to overlapping probes.

            RTN_Open(rtn);
            INS ins = RTN_InsHead(rtn);
            if (!INS_Valid(ins))
            {
                cerr << "Failed to get first instruction of " << RTN_Name(rtn) << endl;
                PIN_ExitProcess(1);
            }

            ins = INS_Next(ins);
            if (!INS_Valid(ins))
            {
                cerr << "Failed to get next instruction after first of " << RTN_Name(rtn) << endl;
                PIN_ExitProcess(1);
            }

            ADDRINT addr = INS_Address(ins);
            RTN_Close(rtn);

            RTN fake_rtn = RTN_CreateAt(addr, "FakeRtn");
            if (RTN_Valid(fake_rtn))
            {
                if (!RTN_InsertCallProbed(fake_rtn, IPOINT_BEFORE, AFUNPTR(before), IARG_END))
                {
                    cerr << "Failed to probe " << RTN_Name(rtn) << endl;
                    PIN_ExitProcess(1);
                }
            }
            else
            {
                cerr << "RTN_CreateAt failed. " << endl;
                PIN_ExitProcess(1);
            }

            // Try to probe the original
            if (RTN_InsertCallProbed(rtn, IPOINT_BEFORE, AFUNPTR(before), IARG_END))
            {
                cerr << RTN_Name(rtn)
                     << "() is expected to be unsafe for probing after inserting overlapping probe with RTN_CreateAt " << endl;
                PIN_ExitProcess(1);
            }
        }
    }
}

/* ===================================================================== */

int main(INT32 argc, CHAR* argv[])
{
    PIN_InitSymbols();

    PIN_Init(argc, argv);

    IMG_AddInstrumentFunction(ImageLoad, 0);
    PIN_StartProgramProbed();

    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
