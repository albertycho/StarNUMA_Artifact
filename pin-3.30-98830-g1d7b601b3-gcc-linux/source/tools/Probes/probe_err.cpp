/*
 * Copyright (C) 2007-2023 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

/*! @file
  This is a negative test!!  It produces an error by not calling
  RTN_IsSafeForProbedReplacement().  Do not do this.
 */

#include "pin.H"
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include "tool_macros.h"
using std::cerr;
using std::cout;
using std::endl;
using std::string;

/* ===================================================================== */
/* Global Variables */
/* ===================================================================== */

KNOB< string > KnobFunctionName(KNOB_MODE_WRITEONCE, "pintool", "func_name", "", "Name of function to replace");

static void (*pf_dn)();

/* ===================================================================== */

INT32 Usage()
{
    cerr << "This pin tool tests probe replacement.\n"
            "\n";
    cerr << KNOB_BASE::StringKnobSummary();
    cerr << endl;
    return -1;
}

void Foo_Function()
{
    if (pf_dn)
    {
        (*pf_dn)();
    }
}

/* ===================================================================== */
// Called every time a new image is loaded
// Look for routines that we want to probe
VOID ImageLoad(IMG img, VOID* v)
{
    if (!IMG_IsMainExecutable(img)) return;
    RTN rtn = RTN_FindByName(img, C_MANGLE(KnobFunctionName.Value().c_str()));
    ASSERTX(RTN_Valid(rtn));

    pf_dn = (void (*)())RTN_ReplaceProbedEx(rtn, PROBE_MODE_ALLOW_RELOCATION, AFUNPTR(Foo_Function));
    if (pf_dn != NULL)
    {
        cout << KnobFunctionName.Value() << " replaced successfully" << endl;
    }
    else
    {
        cout << KnobFunctionName.Value() << " failed to be replaced" << endl;
        PIN_ExitProcess(1);
    }
}

/* ===================================================================== */

int main(int argc, CHAR* argv[])
{
    PIN_InitSymbols();

    if (PIN_Init(argc, argv))
    {
        return Usage();
    }

    IMG_AddInstrumentFunction(ImageLoad, 0);

    PIN_StartProgramProbed();

    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
