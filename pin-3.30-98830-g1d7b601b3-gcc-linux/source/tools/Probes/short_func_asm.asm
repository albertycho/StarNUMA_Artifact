;
; Copyright (C) 2023-2023 Intel Corporation.
; SPDX-License-Identifier: MIT
;

include asm_macros.inc

IFDEF TARGET_IA32
.686
.model flat, c
ENDIF

COMMENT // contains a short function with a size of 2 bytes

.code
PUBLIC short_func
short_func PROC
    nop
    ret
short_func ENDP

; This function is here only to terminate short_func
.code
PUBLIC short_func_terminator
short_func_terminator PROC
    xor GAX_REG, GAX_REG
    xor GAX_REG, GAX_REG
    xor GAX_REG, GAX_REG
    xor GAX_REG, GAX_REG
    ret
short_func_terminator ENDP

END
