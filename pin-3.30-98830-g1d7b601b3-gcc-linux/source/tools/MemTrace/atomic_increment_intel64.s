/*
 * Copyright (C) 2011-2023 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

.global numthreadsStarted
.type AtomicIncrement, @function
.global AtomicIncrement
AtomicIncrement:
    lea     numthreadsStarted(%rip), %rcx
    lock incl     (%rcx)
    ret



