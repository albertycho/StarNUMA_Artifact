/*
 * Copyright (C) 2023-2023 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */
#include <sys/auxv.h>
#include <elf.h>
#include <stdint.h>
#include <stdio.h>

// We use this define to check if the kernel has FSGSBASE instructions enabled and application can use them.
// see kernel documentation at :
// https://www.kernel.org/doc/html/v5.10/x86/x86_64/fsgs.html#accessing-fs-gs-base-with-the-fsgsbase-instructions
// (section 24.8.4.1 - FSGSBASE instructions enabling)
#ifndef HWCAP2_FSGSBASE
#define HWCAP2_FSGSBASE (1 << 1)
#endif

int main()
{
    unsigned val = getauxval(AT_HWCAP2);

    if (val & HWCAP2_FSGSBASE)
    {
        // FS/GS Base instructions are supported.
        uint8_t mop_array[1024];
        int i;
        for (i = 0; i < 1024; ++i)
        {
            mop_array[i] = 0x0;
        }
        uint8_t* seg_base = &mop_array[0] - 0;
        uint8_t* base     = (uint8_t*)(0 + 0);
        uint64_t index    = (uint64_t)(0);
        __asm__ volatile(".byte 0xf3,0x48,0x0f,0xae,0xd9; /* wrgsbase %%rcx    */"
                         ".byte 0x65,0x48,0xff,0x08;      /* decq %%gs:(%%rax) */"
                         :
                         : "a"(base), "c"(seg_base));

        printf("\n[APP] mop_array [0-11]:");
        printf("\n[APP] bytes = ");
        for (i = 0; i < 12; ++i)
        {
            printf("%02x ", mop_array[i]);
        }
        printf("\n[APP] GS segment base address = %p", seg_base);
        printf("\n\n");
    }
    else
    {
        printf("\n[APP] GS/FS Base Instructions are not supported.\n\n");
    }
    return 0;
}
