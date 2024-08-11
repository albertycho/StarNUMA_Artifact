/*
 * Copyright (C) 2023-2023 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void set_mxcsr(unsigned int);
unsigned int get_mxcsr();
extern "C" unsigned fnopproc();

#ifdef TARGET_WINDOWS

#ifdef TARGET_IA32

#define go_fnop() __asm { fnop}

#else  // ~TARGET_IA32

#define go_fnop() fnopproc()

#endif // ~TARGET_IA32

#else // ~TARGET_WINDOWS

#define go_fnop() asm("fnop")

#endif // ~TARGET_WINDOWS

int main(int argc, char** argv)
{
    const unsigned int MXCSR_DEFAULT         = 0x1f80;
    const unsigned int MXCSR_EXPECTED_AT_END = 0x1fe0;

    // save mxcsr register aside
    unsigned int mxcsr = get_mxcsr();
    printf("Start app: mxcsr=0x%x\n", mxcsr);

    if (mxcsr != MXCSR_DEFAULT)
    {
        printf("Unexpected mxcsr value at start stage, exiting...\n");
        return 1;
    }

    // fnop
    go_fnop();

    // check mxcsr register compared to saved value above
    mxcsr = get_mxcsr();
    printf("Middle app: mxcsr=0x%x\n", mxcsr);

    if (mxcsr != MXCSR_DEFAULT)
    {
        printf("Unexpected mxcsr value after first fnop, exiting...\n");
        return 1;
    }

    // second fnop
    go_fnop();

    // check mxcsr register compared to saved value above
    mxcsr = get_mxcsr();
    printf("End app: mxcsr=0x%x\n", mxcsr);
    if (mxcsr != MXCSR_EXPECTED_AT_END)
    {
        printf("Unexpected mxcsr value after end stage, exiting...\n");
        return 1;
    }

    printf("mxcsr value expected ok, test success\n");
    return 0;
}
