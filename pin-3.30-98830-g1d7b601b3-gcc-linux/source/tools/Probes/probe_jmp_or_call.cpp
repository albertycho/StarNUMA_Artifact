/*
 * Copyright (C) 2023-2023 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

#include "pin.H"
#include <iostream>
#include <stdlib.h>
#include "tool_macros.h"

/*
 *  Pintool mode of operation:
 *  o   The pintool instruments two functions: Bar() a second function that is configured via the KnobMode.
 *  o   We expect the analysis routine for the second routine to be called. This is our proof that the instrumentation
 *      for the JMP/CALL worked as expected.  
 *  o   All the functions call Bar(), so we expect the analysis routine for Bar to be called for all the tests.
 *      This is our proof that the second routine operated correctly.
 */

// When this knob is TRUE then the test will not assert on failure but just print.
// This is used for negative tests (that are expected to fail)
// Default: FALSE
static KNOB< BOOL > KnobAllowFailure(KNOB_MODE_WRITEONCE, "pintool", "allow_fail", "0", "Allow failure (do not assert)");

// When this knob is TRUE then the pintool will insert the probe using the PROBE_MODE_ALLOW_POTENTIAL_BRANCH_TARGET mode.
// With this mode, if a potential branch target is detected in the probe area, Pin will still place a probe there.
// Default: FALSE
static KNOB< BOOL > KnobAllowProbePotentialBranchTarget(KNOB_MODE_WRITEONCE, "pintool", "allow_probe_branch_target", "0",
                                                        "Allow probing a potential branch target");

// A number that represents the test mode (which routine is instrumented).
// The application has a mode parameter with identical semantics.
static KNOB< UINT32 > KnobMode(KNOB_MODE_WRITEONCE, "pintool", "mode", "0",
                               "The number represents that function that will be called by the application");

// An enum for controlling the instrumentation point.
// InstPtRtnHead            - instrument at the routine head
// InstPtFirstBranchOrCall  - search for the first branch or call. We can move the instrumentation point further by using the shift field below.
enum InstrumentationPoint
{
    InstPtRtnHead,
    InstPtFirstBranchOrCall
};
struct RtnInstrumentation
{
    std::string rtnName;         // The name of the routine to probe
    PROBE_MODE probeMode;        // The probing mode
    InstrumentationPoint instPt; // The probe point.
    ADDRDELTA
        instPtShift; // Optional shifting of the probe point (can be negative). Applicable only if instPt = InstPtFirstBranchOrCall

    RtnInstrumentation(const char* name) : rtnName(name), probeMode(PROBE_MODE_DEFAULT), instPt(InstPtRtnHead), instPtShift(0) {}
    RtnInstrumentation(const char* name, PROBE_MODE mode) : rtnName(name), probeMode(mode), instPt(InstPtRtnHead), instPtShift(0)
    {}
    RtnInstrumentation(const char* name, InstrumentationPoint pt, INT32 shift)
        : rtnName(name), probeMode(PROBE_MODE_DEFAULT), instPt(pt), instPtShift(shift)
    {}
    RtnInstrumentation(const char* name, PROBE_MODE mode, InstrumentationPoint pt, INT32 shift)
        : rtnName(name), probeMode(mode), instPt(pt), instPtShift(shift)
    {}
};

#ifdef TARGET_IA32E
//
// 64 bit
//
RtnInstrumentation rtnsToInstrument[] = {
    /* 1  - Probe an IP-relative JMP located at byte 0 in the probe area */
    RtnInstrumentation("JmpRelAt0"),
    /* 2  - Probe an IP-relative JMP located at byte 6 in the probe area */
    RtnInstrumentation("JmpRelAt6"),
    /* 3  - Probe a direct JMP located at byte 0 in the probe area */
    RtnInstrumentation("JmpDirectAt0"),
    /* 4  - Probe a direct JMP located at byte 6 in the probe area */
    RtnInstrumentation("JmpDirectAt6"),
    /* 5  - Probe an IP-relative CALL located at byte 0 in the probe area */
    RtnInstrumentation("CallRelAt0"),
    /* 6  - Probe an IP-relative CALL located at byte 6 in the probe area */
    RtnInstrumentation("CallRelAt6"),
    /* 7  - Probe a direct JMP located at byte 0 in the probe area */
    RtnInstrumentation("CallDirectAt0"),
    /* 8  - Probe a direct JMP where we have a branch into the probe area (required relocation) */
    RtnInstrumentation("JmpTargetInProbe", PROBE_MODE_ALLOW_RELOCATION),
    /* 9  - Probe a direct JMP where we have an internal branch in the probe area (required relocation) */
    RtnInstrumentation("JmpDirectToNext", PROBE_MODE_ALLOW_RELOCATION),
    /* 10 - Probe an register based indirect CALL located at byte 0 in the probe area */
    RtnInstrumentation("CallIndirectReg", InstPtFirstBranchOrCall, 0),
    /* 11 - Probe an register based indirect JMP located at byte 0 in the probe area */
    RtnInstrumentation("JmpIndirectReg", InstPtFirstBranchOrCall, 0),
    /* 12 - Probe an register based indirect CALL located at byte 4 in the probe area */
    RtnInstrumentation("CallIndirectReg", InstPtFirstBranchOrCall, -3)};
#else
//
// 32 bit
//
RtnInstrumentation rtnsToInstrument[] = {
    /* 1 - Probe an IP-relative JMP located at byte 0 in the probe area */
    RtnInstrumentation("CallDirectAt0"),
    /* 2 - Probe an IP-relative JMP located at byte 4 in the probe area */
    RtnInstrumentation("CallDirectAt5"),
    /* 3 - Probe an register based indirect CALL located at byte 0 in the probe area */
    RtnInstrumentation("CallIndirectReg", InstPtFirstBranchOrCall, 0),
    /* 4 - Probe an register based indirect JMP located at byte 0 in the probe area */
    RtnInstrumentation("JmpIndirectReg", InstPtFirstBranchOrCall, 0),
    /* 5 - Probe a direct JMP where we have a branch into the probe area (required relocation) */
    RtnInstrumentation("JmpTargetInProbe", PROBE_MODE_ALLOW_RELOCATION),
    /* 6 - Probe a direct JMP where we have an internal branch in the probe area (required relocation) */
    RtnInstrumentation("JmpDirectToNext", PROBE_MODE_ALLOW_RELOCATION),
    /* 7 - Probe an register based indirect CALL located at byte 4 in the probe area */
    RtnInstrumentation("CallIndirectReg", InstPtFirstBranchOrCall, -3)};
#endif // TARGET_IA32E

STATIC VOID PrintRoutineDisasm(RTN rtn, ADDRINT highlightAddress)
{
    RTN_Open(rtn);

    std::cout << RTN_Name(rtn) << ":" << std::endl;
    for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins))
    {
        std::cout << (INS_Address(ins) == highlightAddress ? " --> " : "     ") << std::dec << "(" << INS_Size(ins) << ") "
                  << std::hex << INS_Address(ins) << " : " << INS_Disassemble(ins) << std::endl;
    }

    RTN_Close(rtn);
}

VOID BeforeRtn() { std::cout << "Before function " << rtnsToInstrument[KnobMode.Value() - 1].rtnName << std::endl; }

VOID BeforeBar() { std::cout << "Before Bar" << std::endl; }

VOID ImageLoad(IMG img, VOID* v)
{
    if (IMG_IsMainExecutable(img) == FALSE) return;

    // Find the routine to instrument
    UINT32 mode         = KnobMode.Value() - 1;
    std::string rtnName = rtnsToInstrument[mode].rtnName;
    RTN rtn             = RTN_FindByName(img, rtnName.c_str());
    ASSERT(RTN_Valid(rtn), "Failed to find routine " + rtnName + "\n");

    // The default instrumentation point is at the routine head. We may move it further depending on instPt.
    ADDRINT probeAddress = RTN_Address(rtn);

    // If the instrumentation point is not at the routine head but rather on the first branch instruction
    // then we should search for the first JMP or CALL.
    // Depending on the shift value (instPtShift) we may move the instrumentation point further, relative
    // to the branch instruction, into one of the instructions before/after the branch instruction.
    // Once the new instrumentation point has been finalized, we create a new RTN on that instruction
    // and place the probe there.
    if (rtnsToInstrument[mode].instPt == InstPtFirstBranchOrCall)
    {
        RTN_Open(rtn);

        // Move forward to find Branch or Call
        INS insToInstrument = INS_Invalid();
        for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins))
        {
            if (INS_IsBranch(ins) || INS_IsCall(ins))
            {
                insToInstrument = ins;
                break;
            }
        }
        ASSERTX(INS_Valid(insToInstrument));

        // Move forward or backwards from the branch instruction.
        // We must be aligned on an instruction.
        // The purpose is to locate the probe point that will include the branch instruction
        // but not necessarilly at the beginning of the probe area.
        if (rtnsToInstrument[mode].instPtShift != 0)
        {
            INS insShifted = INS_Invalid();
            if (rtnsToInstrument[mode].instPtShift > 0) // move forward
            {
                for (INS ins = INS_Next(insToInstrument); INS_Valid(ins); ins = INS_Next(ins))
                {
                    ADDRDELTA delta = INS_Address(ins) - INS_Address(insToInstrument);
                    if (delta == rtnsToInstrument[mode].instPtShift)
                    {
                        insShifted = ins;
                        break;
                    }
                }
            }
            else if (rtnsToInstrument[mode].instPtShift < 0) // move backwards
            {
                for (INS ins = INS_Prev(insToInstrument); INS_Valid(ins); ins = INS_Prev(ins))
                {
                    ADDRDELTA delta = INS_Address(insToInstrument) - INS_Address(ins);
                    if (delta == -rtnsToInstrument[mode].instPtShift)
                    {
                        insShifted = ins;
                        break;
                    }
                }
            }
            ASSERTX(INS_Valid(insShifted)); // make sure we found a valid isntruction at the expected offset
            insToInstrument = insShifted;
        }
        probeAddress = INS_Address(insToInstrument);

        RTN_Close(rtn);
    }
    ASSERTX(probeAddress != 0);

    PrintRoutineDisasm(rtn, probeAddress);

    // Create a new routine at the instrumentation point in case it is not at the original routine head
    if (probeAddress != RTN_Address(rtn))
    {
        RTN newRtn = RTN_CreateAt(probeAddress, "RtnAtBranch");
        ASSERTX(RTN_Valid(newRtn));
        rtn = newRtn;
    }

    if (KnobAllowProbePotentialBranchTarget)
    {
        rtnsToInstrument[mode].probeMode =
            (PROBE_MODE)(rtnsToInstrument[mode].probeMode | PROBE_MODE_ALLOW_POTENTIAL_BRANCH_TARGET);
    }

    // First try to replace with default probe mode.
    // If it fails and PROBE_MODE_ALLOW_RELOCATION/PROBE_MODE_ALLOW_POTENTIAL_BRANCH_TARGET for this test then try to replace using that mode.
    BOOL probed = RTN_InsertCallProbed(rtn, IPOINT_BEFORE, AFUNPTR(BeforeRtn), IARG_END);
    if (!probed && (rtnsToInstrument[mode].probeMode != PROBE_MODE_DEFAULT))
    {
        std::cout << "Failed to instrument " + rtnName + " in default probe mode" << std::endl;
        probed = RTN_InsertCallProbedEx(rtn, IPOINT_BEFORE, rtnsToInstrument[mode].probeMode, AFUNPTR(BeforeRtn), IARG_END);
    }
    if (probed)
    {
        std::cout << rtnName + " instrumented successfully at " << std::hex << RTN_Address(rtn) << std::endl;

        // Instrument Bar
        RTN rtnBar = RTN_FindByName(img, "Bar");
        ASSERT(RTN_Valid(rtnBar), "Failed to find routine Bar \n");
        ASSERT(RTN_InsertCallProbed(rtnBar, IPOINT_BEFORE, AFUNPTR(BeforeBar), IARG_END), "Failed to instrument Bar \n");
        std::cout << "Bar instrumented successfully at " << std::hex << RTN_Address(rtnBar) << std::endl;
    }
    else
    {
        if (KnobAllowFailure)
        {
            std::cout << "Failed to instrument " << rtnName << " at " << std::hex << RTN_Address(rtn) << std::endl;
        }
        else
        {
            ASSERT(probed, "Failed to instrument " + rtnName + " at " + hexstr(RTN_Address(rtn)) + "\n");
        }
    }
}

int main(INT32 argc, CHAR* argv[])
{
    PIN_InitSymbols();
    if (PIN_Init(argc, argv))
    {
        std::cerr << "PIN_Init failed" << std::endl;
        return 1;
    }
    if ((KnobMode.Value() == 0) || (KnobMode.Value() > sizeof(rtnsToInstrument) / sizeof(rtnsToInstrument[0])))
    {
        std::cerr << "Illegal mode " << std::dec << KnobMode << std::endl;
        PIN_ExitProcess(-1);
    }
    IMG_AddInstrumentFunction(ImageLoad, NULL);
    PIN_StartProgramProbed();
    return 0;
}
