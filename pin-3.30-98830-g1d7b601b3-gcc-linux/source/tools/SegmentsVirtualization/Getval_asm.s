/*
 * Copyright (C) 2023-2023 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */


.global GetValAsm
.type GetValAsm, @function

GetValAsm:
   push %edx
   mov 8(%ebp), %edx
   mov %fs:0x0(%edx), %edx
   mov %edx, %eax
   pop %edx
   ret
   
   #mov $0xffffffff88888888, %rax
   #mov %gs, %eax
   #ret
