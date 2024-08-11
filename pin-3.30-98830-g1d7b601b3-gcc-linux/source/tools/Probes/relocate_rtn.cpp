/*
 * Copyright (C) 2009-2023 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

/* ===================================================================== */
/*! @file
  This pintool checls function replacement for functions that require relocation.
  The tool verifies the functions cannot be replaced without relocation, and are successfully replaced with relocation.
  The tool also verifies that function replacement is not possible either with
  or without relocaton for functions that are not relocatable.
 */

#include <stdlib.h>
#include "pin.H"
#include <iostream>
#include <fstream>
#include "tool_macros.h"
using std::cerr;
using std::cout;
using std::endl;

/* ===================================================================== */
/* Global Variables */
/* ===================================================================== */

typedef void (*FUNCPTR)();
STATIC FUNCPTR pf_r2    = NULL;

/* ===================================================================== */

INT32 Usage()
{
    cerr << "This pin tool replaces relocatable_1() and relocatable_2()\n"
            "\n";
    cerr << KNOB_BASE::StringKnobSummary();
    cerr << endl;
    return -1;
}

/* ===================================================================== */

void R2()
{
    cout << "R2: calling original relocatable_2()" << endl;

    return (pf_r2)();
}

void R3(FUNCPTR fptr)
{
    cout << "R3: calling original relocatable_3()" << endl;

    return (fptr)();
}

void DummyReplacement() { ASSERT(FALSE, "We should not get here"); }

/* ===================================================================== */

AFUNPTR ReplaceRelocatable(IMG img, const char* fname, AFUNPTR replacementFunc)
{
    RTN rtn = RTN_FindByName(img, fname);
    ASSERT(RTN_Valid(rtn), fname + " not found in " + IMG_Name(img));

    AFUNPTR origFptr = (FUNCPTR)RTN_ReplaceProbed(rtn, replacementFunc);
    ASSERT((origFptr == NULL), std::string(fname) + " routine should not be probed w/o relocation");

    origFptr = (FUNCPTR)RTN_ReplaceProbedEx(rtn, PROBE_MODE_ALLOW_RELOCATION, replacementFunc);
    ASSERT((origFptr != NULL), "ImageLoad: Pin does not want to replace " + fname + " in:" + IMG_Name(img));
    cout << "ImageLoad: Replaced " << fname << " in:" << IMG_Name(img) << endl;

    return origFptr;
}

VOID ReplaceSignatureRelocatable(IMG img, const char* fname, AFUNPTR replacementFunc)
{
    RTN rtn = RTN_FindByName(img, fname);
    ASSERT(RTN_Valid(rtn), fname + " not found in " + IMG_Name(img));

    PROTO proto      = PROTO_Allocate(PIN_PARG(void), CALLINGSTD_DEFAULT, fname, PIN_PARG_END());
    AFUNPTR origFptr = RTN_ReplaceSignatureProbed(rtn, replacementFunc, IARG_PROTOTYPE, proto, IARG_ORIG_FUNCPTR, IARG_END);
    ASSERT((origFptr == NULL), std::string(fname) + " routine should not be probed w/o relocation");

    origFptr = RTN_ReplaceSignatureProbedEx(rtn, PROBE_MODE_ALLOW_RELOCATION, replacementFunc, IARG_PROTOTYPE, proto,
                                            IARG_ORIG_FUNCPTR, IARG_END);
    ASSERT((origFptr != NULL), "ImageLoad: Pin does not want to replace " + fname + " in:" + IMG_Name(img));
    cout << "ImageLoad: Replaced " << fname << " in:" << IMG_Name(img) << endl;
}

VOID CheckNonRelocatable(const char* fname, IMG img)
{
    RTN rtn = RTN_FindByName(img, fname);
    ASSERT(RTN_Valid(rtn), fname + " not found in " + IMG_Name(img));

    AFUNPTR origFptr = (FUNCPTR)RTN_ReplaceProbed(rtn, (AFUNPTR)DummyReplacement);
    ASSERT((origFptr == NULL), std::string(fname) + " routine should not be probed w/o relocation");

    origFptr = (FUNCPTR)RTN_ReplaceProbedEx(rtn, PROBE_MODE_ALLOW_RELOCATION, (AFUNPTR)DummyReplacement);
    ASSERT((origFptr == NULL), std::string(fname) + " routine should not be probed with relocation");
    cout << "ImageLoad: Pin behavior is right: reject to replace " << fname << " routine" << endl;
}

VOID ImageLoad(IMG img, VOID* v)
{
    if (IMG_IsMainExecutable(img))
    {
        pf_r2  = (FUNCPTR)ReplaceRelocatable(img, C_MANGLE("relocatable_2"), AFUNPTR(R2));
        ReplaceSignatureRelocatable(img, C_MANGLE("relocatable_3"), AFUNPTR(R3));
        CheckNonRelocatable(C_MANGLE("non_relocatable_1"), img);
        CheckNonRelocatable(C_MANGLE("non_relocatable_2"), img);
    }
}

/* ===================================================================== */

int main(int argc, CHAR* argv[])
{
    PIN_InitSymbols();

    if (PIN_Init(argc, argv)) return Usage();

    IMG_AddInstrumentFunction(ImageLoad, 0);

    PIN_StartProgramProbed();

    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
