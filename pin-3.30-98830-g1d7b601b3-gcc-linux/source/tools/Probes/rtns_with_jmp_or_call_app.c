/*
 * Copyright (C) 2023-2023 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

/*
 * This application calls functions that contain either JMP or CALL in the first bytes of the routine.
 * The purpose is to test placing a probe on bytes that contains JMP/CALL.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

typedef void (*fptr)();

typedef struct _RtnDispatch
{
    fptr f;
    const char* fname;
} RtnDispatch;

#ifdef TARGET_IA32E
//
// 64 bit
//
#ifdef TARGET_LINUX
// Implemented in the assembly file
void JmpRelAt0();
void JmpRelAt6();
void CallRelAt0();
void CallRelAt6();
#else
// Implement as stubs. We need the stubs becaue there is a dispatch table below that expects 11 entries.
// The relevant pintool probe_jmp_or_call.cpp expects the same number of entries in the same order.
// On Windows they are stubs because we don't have implementation for IP relative jmp or call in the tests.
void JmpRelAt0() {}
void JmpRelAt6() {}
void CallRelAt0() {}
void CallRelAt6() {}
#endif
void JmpDirectAt0();
void JmpDirectAt6();
void CallDirectAt0();
void JmpTargetInProbe();
void JmpDirectToNext();
void CallIndirectReg();
void JmpIndirectReg();

const RtnDispatch dispatch_table[] = {
    {JmpRelAt0, "JmpRelAt0"},               // 1
    {JmpRelAt6, "JmpRelAt6"},               // 2
    {JmpDirectAt0, "JmpDirectAt0"},         // 3
    {JmpDirectAt6, "JmpDirectAt6"},         // 4
    {CallRelAt0, "CallRelAt0"},             // 5
    {CallRelAt6, "CallRelAt6"},             // 6
    {CallDirectAt0, "CallDirectAt0"},       // 7
    {JmpTargetInProbe, "JmpTargetInProbe"}, // 8
    {JmpDirectToNext, "JmpDirectToNext"},   // 9
    {CallIndirectReg, "CallIndirectReg"},   // 10
    {JmpIndirectReg, "JmpIndirectReg"}      // 11
};

const char* helpStr = "Usage:\trtns_with_jmp_or_call.exe -mode <mode> \n"
                      "  Modes: \n"
                      "  1 - JMP to Bar, ip relative, JMP at offset 0 from routine base \n"
                      "  2 - JMP to Bar, ip relative, JMP at offset 6 from routine base \n"
                      "  3 - JMP to Bar, absolute offset, JMP at offset 0 from routine base \n"
                      "  4 - JMP to Bar, absolute offset, JMP at offset 6 from routine base \n"
                      "  5 - CALL to Bar, ip relative, CALL at offset 0 from routine base \n"
                      "  6 - CALL to Bar, ip relative, CALL at offset 6 from routine base \n"
                      "  7 - CALL to Bar, direct, CALL at offset 0 from routine base \n"
                      "  8 - CALL to Bar, direct, branch target in probe area \n"
                      "  9 - JMP to the next instruction. branch target in probe area \n"
                      "  10 - CALL direct using a register to Bar \n"
                      "  11 - JMP direct using a register to Bar \n";
#else
//
// 32 bit
//
void CallDirectAt0();
void CallDirectAt5();
void CallIndirectReg();
void JmpIndirectReg();
void JmpTargetInProbe();
void JmpDirectToNext();

const RtnDispatch dispatch_table[] = {
    {CallDirectAt0, "CallDirectAt0"},       // 1
    {CallDirectAt5, "CallDirectAt5"},       // 2
    {CallIndirectReg, "CallIndirectReg"},   // 3
    {JmpIndirectReg, "JmpIndirectReg"},     // 4
    {JmpTargetInProbe, "JmpTargetInProbe"}, // 5
    {JmpDirectToNext, "JmpDirectToNext"}    // 6
};

const char* helpStr = "Usage:\trtns_with_jmp_or_call.exe -mode <mode> \n"
                      "  Modes: \n"
                      "  1 - CALL to Bar, direct, CALL at offset 0 from routine base \n"
                      "  2 - CALL to Bar, direct, CALL at offset 5 from routine base \n"
                      "  3 - CALL to Bar, indirect \n"
                      "  4 - JMP to Bar, indirect \n"
                      "  5 - JMP to Bar, direct, branch target in probe area \n"
                      "  6 - JMP to the next instruction. branch target in probe area \n";
#endif // TARGET_IA32E

int main(int argc, char* argv[])
{
    const char* optMode = "-mode";
    int mode            = 0; // 0 is illegal mode
    if (argc == 3)
    {
        int i;
        for (i = 0; i < argc;)
        {
            if (strcmp(argv[i], optMode) == 0)
            {
                i++;
                assert(i < argc);
                mode = atoi(argv[i]);
            }
            i++;
        }
    }

    if ((mode == 0) || (mode > (sizeof(dispatch_table) / sizeof(dispatch_table[0]))))
    {
        fprintf(stderr, "ERROR - illegal mode\n");
        fprintf(stderr, "%s\n", helpStr);
        return 1;
    }

    fprintf(stdout, "Calling %s() \n", dispatch_table[mode - 1].fname);
    dispatch_table[mode - 1].f();
    fprintf(stdout, "Returned from %s() \n", dispatch_table[mode - 1].fname);
    return 0;
}
