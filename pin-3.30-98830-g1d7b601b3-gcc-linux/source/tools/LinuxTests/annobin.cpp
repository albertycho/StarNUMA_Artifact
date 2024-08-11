/*
 * Copyright (C) 2004-2023 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

/*! @file
 *  This file is used to test that pin can parse annobin string section.
 */

#include "pin.H"
#include <iostream>
#include <fstream>

using namespace std;

/* ===================================================================== */
/* Commandline Switches */
/* ===================================================================== */

KNOB< string > KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "annobin.out", "output  file name");
ofstream out;

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32 Usage()
{
    cerr << "This pin tool test annobin annotations\n";
    cerr << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}

/* ===================================================================== */
/* Global Variables */
/* ===================================================================== */

/* ===================================================================== */

VOID ImageLoad(IMG img, VOID* v)
{
    BOOL found = false;
    out << "Image: " << IMG_Name(img) << endl;

    // Visit all the sections of the image that are executable
    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec))
    {
        string name = SEC_Name(sec);
        if (name.find("annobin") != string::npos)
        {
            out << " Found Section: " << SEC_Name(sec) << endl;
            found = true;
        }
    }
    if (!found)
    {
        out << " Not found" << endl;
    }
}

VOID Fini(int, VOID* v)
{
    out << "# eof" << endl;
    out.close();
}

/* ===================================================================== */
/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, CHAR* argv[])
{
    PIN_InitSymbols();

    if (PIN_Init(argc, argv))
    {
        return Usage();
    }

    IMG_AddInstrumentFunction(ImageLoad, 0);

    PIN_AddFiniFunction(Fini, 0);

    out.open(KnobOutputFile.Value().c_str());

    // Never returns
    PIN_StartProgram();

    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
