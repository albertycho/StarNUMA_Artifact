;
; Copyright (C) 2011-2023 Intel Corporation.
; SPDX-License-Identifier: MIT
;

PUBLIC AtomicIncrement


.686
.model flat, c
extern numthreadsStarted:dword
.code
 ALIGN 4 
 AtomicIncrement PROC
    lea ecx, numthreadsStarted
    lock inc DWORD PTR [ecx]
    ret
AtomicIncrement ENDP

end
