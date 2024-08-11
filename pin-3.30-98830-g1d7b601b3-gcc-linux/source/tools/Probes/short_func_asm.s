/*
 * Copyright (C) 2023-2023 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

#include <asm_macros.h>

# contains a short function with a size of 2 bytes

.text
.align 4
.global NAME(short_func)
DECLARE_FUNCTION(short_func)

NAME(short_func):
    nop
    ret

# This function is here only to terminate short_func
.text
.align 4
.global NAME(short_func_terminator)
DECLARE_FUNCTION(short_func_terminator)

NAME(short_func_terminator):
    xor GAX_REG, GAX_REG
    xor GAX_REG, GAX_REG
    xor GAX_REG, GAX_REG
    xor GAX_REG, GAX_REG
    ret

