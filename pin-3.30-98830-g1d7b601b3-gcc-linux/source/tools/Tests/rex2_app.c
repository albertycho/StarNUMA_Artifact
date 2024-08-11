/*
 * Copyright (C) 2023-2023 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>

int main()
{
    int a = 1;
    printf("calling rex2 instruction\n");
    // rex2 add rax rax -> d5 08 01 c0
    asm volatile(".byte 0xd5, 0x08, 0x01, 0xc0 " : "=a"(a) : "a"(a));

    // Under pin the above instruction will be executed w/o the rex2
    if (a == 2) return 0;

    return 1;
}
