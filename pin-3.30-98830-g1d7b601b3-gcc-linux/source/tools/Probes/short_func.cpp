/*
 * Copyright (C) 2023-2023 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

/*
 * This tool is testing the 1-byte probe, where only the first byte of the probe is modified into
 * a direct jmp opcode 0xe9 and the consequent 4 bytes are used unmodified as a random offset.
 *
 * Pintool mode of operation:
 *  The pintool tries to instrument one function "short_func". 
 *  This function is 2-bytes in size and is too small for probing in all probe sizes except with the 1-byte probe.
 *  The function is instrumented according to the mode (Insert / Replace / ReplaceSignature).
 *  The pintool expects to find "short_func" in the main image or else it would fail.
 *  It is possible that instrumentation will not succeed - and this is not a failure.
 *  Instrumentation may fail due to : (1) the 1-byte probe is disabled, or (2) the random address is already mapped,
 *  or (3) the random address evaluates to a displacement that exceeds a 32-bit signed offset.
 *  However, if instrumentation succeeds, the tool validates the following:
 *      o The probe modified the first byte only
 *      o The IsSafe* functions returned TRUE
 */

#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include "pin.H"
#include "tool_macros.h"
using std::cerr;
using std::cout;
using std::endl;
using std::hex;
using std::ios;
using std::ostream;
using std::string;

// 1 RTN_InsertCallProbed
// 2 RTN_ReplaceProbed
// 3 RTN_ReplaceSignatureProbed
constexpr UINT32 ModeInsert     = 1;
constexpr UINT32 ModeReplace    = 2;
constexpr UINT32 ModeReplaceSig = 3;
KNOB< UINT32 > KnobMode(KNOB_MODE_WRITEONCE, "pintool", "mode", decstr(ModeInsert),
                        "1=RTN_InsertCallProbed (default), 2=RTN_ReplaceProbed, 3=RTN_ReplaceSignatureProbed");

KNOB< string > KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "", "specify output file name");

// A struct for storing a sequence of bytes
template< USIZE numBytes > struct RtnBytes
{
    UINT8 bytes[numBytes];
    RtnBytes() { memset((void*)(&bytes[0]), 0, numBytes); }
    VOID load(VOID* address)
    {
        ASSERTX(address != nullptr);
        memcpy((void*)(&bytes[0]), address, numBytes);
    }
    std::vector< UINT8 > GetBytes(UINT32 i1, UINT32 i2)
    {
        ASSERTX((i1 < numBytes) && (i2 < numBytes));
        std::vector< UINT8 > ret;
        for (UINT32 i = i1; i <= i2; i++)
        {
            ret.push_back(bytes[i]);
        }
        return ret;
    }
    std::string toString()
    {
        std::stringstream sstr;
        for (UINT8 byte : bytes)
        {
            sstr << hex << (int)byte << " ";
        }
        return sstr.str();
    }
};

ostream* Out = NULL;
STATIC void (*fnReplaced)();           // A pointer to the replaced function
STATIC ADDRINT rtnAddress         = 0; // The instrumented routine address
constexpr USIZE numBytesToCompare = 5;
STATIC RtnBytes< numBytesToCompare > rtnBytesBefore; // The first routine bytes before instrumentation

INT32 Usage()
{
    cerr << "This pin tool instruments a very short function (2 bytes long).\n" << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}

STATIC VOID StoreRtnBytesBeforeInstrumentation(RTN rtn)
{
    // Save the first bytes of the routine before instrumentation
    rtnAddress = RTN_Address(rtn);
    rtnBytesBefore.load(Addrint2VoidStar(rtnAddress));
    *Out << "Before instrumentation " << hex << rtnAddress << " : " << rtnBytesBefore.toString() << endl;
}

STATIC VOID ValidateRtnBytesAfterInstrumentation()
{
    // Read the first bytes of the routine after instrumentation
    RtnBytes< numBytesToCompare > rtnBytesAfter;
    rtnBytesAfter.load(Addrint2VoidStar(rtnAddress));
    *Out << "After instrumentation  " << hex << rtnAddress << " : " << rtnBytesAfter.toString() << endl;

    // The first byte is expected to change to direct jmp opcode
    {
        if (rtnBytesBefore.GetBytes(0, 0) == rtnBytesAfter.GetBytes(0, 0))
        {
            *Out << "ERROR: Expected the routine first byte to change after instrumentation" << endl;
            PIN_ExitProcess(1);
        }
    }

    // The remaining 4 bytes are expected not to change, they are a random offset
    {
        if (rtnBytesBefore.GetBytes(1, numBytesToCompare - 1) != rtnBytesAfter.GetBytes(1, numBytesToCompare - 1))
        {
            *Out << "ERROR: Unexpected change in the bytes 1-4 of the routine" << endl;
            PIN_ExitProcess(1);
        }
    }
}

void shortFuncReplacement()
{
    *Out << __FUNCTION__ << endl;
    ValidateRtnBytesAfterInstrumentation();
    fnReplaced();
}

STATIC VOID BeforeFunc()
{
    *Out << __FUNCTION__ << endl;
    ValidateRtnBytesAfterInstrumentation();
}

STATIC VOID InsertBefore(RTN rtn)
{
    *Out << "Inserting before " << RTN_Name(rtn) << endl;

    BOOL isSafe = RTN_IsSafeForProbedInsertion(rtn);
    BOOL ok     = RTN_InsertCallProbed(rtn, IPOINT_BEFORE, AFUNPTR(BeforeFunc), IARG_END);
    if (!ok)
    {
        *Out << "Failed to instrument function " << RTN_Name(rtn) << endl;
        PIN_ExitProcess(0); // return with 0 - not a failure
    }
    *Out << "Successfully instrumented function " << RTN_Name(rtn) << endl;
    if (!isSafe)
    {
        *Out << "ERROR: RTN_IsSafeForProbedInsertion returned FALSE for " << RTN_Name(rtn) << endl;
        PIN_ExitProcess(1);
    }
}

STATIC VOID Replace(RTN rtn, BOOL replaceSig)
{
    *Out << "Replacing" << (replaceSig ? " (signature) " : " ") << RTN_Name(rtn) << endl;

    BOOL isSafe = RTN_IsSafeForProbedReplacement(rtn);
    if (replaceSig)
    {
        PROTO proto = PROTO_Allocate(PIN_PARG(void), CALLINGSTD_DEFAULT, "", PIN_PARG_END());
        fnReplaced  = (void (*)())RTN_ReplaceSignatureProbed(rtn, AFUNPTR(shortFuncReplacement), IARG_PROTOTYPE, proto, IARG_END);
    }
    else
    {
        fnReplaced = (void (*)())RTN_ReplaceProbed(rtn, AFUNPTR(shortFuncReplacement));
    }
    if (fnReplaced == NULL)
    {
        *Out << "Failed to instrument function " << RTN_Name(rtn) << endl;
        PIN_ExitProcess(0); // return with 0 - not a failure
    }
    *Out << "Successfully instrumented function " << RTN_Name(rtn) << endl;
    if (!isSafe)
    {
        *Out << "RTN_IsSafeForProbedReplacement returned FALSE for " << RTN_Name(rtn) << endl;
        PIN_ExitProcess(1);
    }
}

VOID ImageLoad(IMG img, VOID* v)
{
    if (!IMG_IsMainExecutable(img)) return;

    const char* funcName = "short_func";
    RTN rtn              = RTN_FindByName(img, funcName);
    if (!RTN_Valid(rtn))
    {
        *Out << "Failed to find function " << funcName << endl;
        PIN_ExitProcess(1);
    }

    StoreRtnBytesBeforeInstrumentation(rtn);

    switch (KnobMode.Value())
    {
        case ModeInsert:
            InsertBefore(rtn);
            break;

        case ModeReplace:
        case ModeReplaceSig:
        {
            const BOOL replaceSig = (KnobMode.Value() == ModeReplaceSig);
            Replace(rtn, replaceSig);
            // With Replace the probe bytes are written at instrumentation so we can verify the bytes here
            ValidateRtnBytesAfterInstrumentation();
        }
        break;
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
    Out = KnobOutputFile.Value().empty() ? &cout : new std::ofstream(KnobOutputFile.Value().c_str());

    IMG_AddInstrumentFunction(ImageLoad, 0);
    PIN_StartProgramProbed();
    return 0;
}
