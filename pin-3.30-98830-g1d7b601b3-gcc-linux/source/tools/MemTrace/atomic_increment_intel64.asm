;
; Copyright (C) 2011-2023 Intel Corporation.
; SPDX-License-Identifier: MIT
;

PUBLIC AtomicIncrement

extern numthreadsStarted:dword
.code
AtomicIncrement PROC
    lea rcx, numthreadsStarted
    lock inc DWORD PTR [rcx]
    ret
AtomicIncrement ENDP


end
