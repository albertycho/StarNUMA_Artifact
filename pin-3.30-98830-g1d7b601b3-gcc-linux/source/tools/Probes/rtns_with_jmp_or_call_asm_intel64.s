/*
 * Copyright (C) 2023-2023 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

#include <asm_macros.h>

# The functions in this file call Bar() either using CALL or JMP.
# Each function has different encoding for the JMP/CALL: 
# either ip-relative, or direct, or indirect using a register.
# The JMP/CALL are placed either at the head of the function or
# a few bytes into the function.
# All functions must call Bar(). 
# The purpose of the different variants is to test probing where the JMP/CALL
# overlap with the probe area.

#
# FUNCTION: Bar()
#
.text
.global NAME(Bar)
DECLARE_FUNCTION(Bar)

NAME(Bar):
    pushq   %rbp
    movq    %rsp, %rbp
    leave
    ret

#
# pf: an object in the text section containing the address of Bar
#
.globl pf
    .text
    .align 8
    .type   pf, @object
    .size   pf, 8
pf:
    .quad   Bar

#
# FUNCTION: CallRelAt0()
# This function calls Bar() using an ip-relative CALL instruction that is placed
# at offset 0 from the function base.
#
# Placing a 6-byte probe on this routine is expected to succeed.
#
# <CallRelAt0>:
#   ff 15 f2 ff ff ff       call   *-0xe(%rip) 
#
.text
.global NAME(CallRelAt0)
DECLARE_FUNCTION(CallRelAt0)

NAME(CallRelAt0):
    call    *(pf-.-6)(%rip)  # 6 = size of this call instruction
    ret
    nop # Pad with NOPs to space the functions
    nop
    nop
    nop
    nop
    nop
	
#
# pf1: an object in the text section containing the address of Bar
#
.globl pf1
    .text
    .align 8
    .type   pf1, @object
    .size   pf1, 8
pf1:
    .quad   Bar

#
# FUNCTION: CallRelAt6()
# This function calls Bar() using an ip-relative CALL instruction that is placed
# at offset 6 from the function base.
#
# Placing a 6-byte probe on this routine is expected to succeed.
#
# <CallRelAt6>:
#   48 85 c9                test   %rcx,%rcx
#   90                      nop
#   90                      nop
#   ff 15 ed ff ff ff       call   *-0x13(%rip)
#   c3                      ret
.text
.global NAME(CallRelAt6)
DECLARE_FUNCTION(CallRelAt6)

NAME(CallRelAt6):
    test %rcx, %rcx
    nop
    nop
    call    *(pf1-.-6)(%rip)  # 6 = size of this call instruction
    ret
    nop # Pad with NOPs to space the functions
    nop
    nop
    nop
    nop
    nop

#
# FUNCTION: CallDirectAt0()
# This function calls Bar() using a direct CALL instruction that is placed
# at offset 0 from the function base.
#
# Placing a 6-byte probe on this routine is expected to fail.
#
# <CallDirectAt0>:
#  e8 c1 ff ff ff          call   <Bar>
#  c3                      ret
#
.text
.global NAME(CallDirectAt0)
DECLARE_FUNCTION(CallDirectAt0)

NAME(CallDirectAt0):
    call    Bar
    ret
    nop # Pad with NOPs to space the functions
    nop
    nop
    nop
    nop
    nop
	

#
# pt: an object in the text section containing the address of Bar
#
.globl pt
    .text
    .align 8
    .type   pt, @object
    .size   pt, 8
pt:
    .quad   Bar

#
# FUNCTION: JmpRelAt0()
# This function calls Bar() using an ip-relative JMP instruction that is placed
# at offset 0 from the function base.
#
# Placing a 6-byte probe on this routine is expected to succeed.
#
# <JmpRelAt0>:
#   ff 25 f2 ff ff ff       jmp    *-0xe(%rip)
#
.text
.global NAME(JmpRelAt0)
DECLARE_FUNCTION(JmpRelAt0)

NAME(JmpRelAt0):
    jmp    *(pt-.-6)(%rip)  # 6 = size of this instruction
    nop # Pad with NOPs to space the functions
    nop
    nop
    nop
    nop
    nop

#
# pt2: an object in the text section containing the address of Bar
#
.globl pt2
    .text
    .align 8
    .type   pt2, @object
    .size   pt2, 8
pt2:
    .quad   Bar

#
# FUNCTION: JmpRelAt6()
# This function calls Bar() using an ip-relative JMP instruction that is placed
# at offset 6 from the function base.
#
# Placing a 6-byte probe on this routine is expected to succeed.
#
# <JmpRelAt6>:
#   48 85 c9                test   %rcx,%rcx
#   90                      nop
#   90                      nop
#   ff 25 ed ff ff ff       jmp    *-0x13(%rip)
#
.text
.global NAME(JmpRelAt6)
DECLARE_FUNCTION(JmpRelAt6)

NAME(JmpRelAt6):
    test %rcx, %rcx
    nop
    nop
    jmp    *(pt2-.-6)(%rip)  # 6 = size of this instruction
    test %rcx, %rcx
    test %rcx, %rcx
    nop # Pad with NOPs to space the functions
    nop
    nop
    nop
    nop
    nop

#
# FUNCTION: JmpDirectAt0()
# This function calls Bar() using a direct JMP instruction that is placed
# at offset 0 from the function base.
#
# Placing a 5-byte/6-byte probe on this routine is expected to succeed.
#
# <JmpDirectAt0>:
#   e9 7b ff ff ff       	jmp    <Bar>
#   48 85 c9             	test   %rcx,%rcx
#   48 85 c9             	test   %rcx,%rcx
#   c3                   	ret
#
.text
.global NAME(JmpDirectAt0)
DECLARE_FUNCTION(JmpDirectAt0)

NAME(JmpDirectAt0):
    jmp    Bar
    test %rcx, %rcx
    test %rcx, %rcx
    ret
    nop # Pad with NOPs to space the functions
    nop
    nop
    nop
    nop
    nop

#
# FUNCTION: JmpDirectAt6()
# This function calls Bar() using a direct JMP instruction that is placed
# at offset 6 from the function base.
#
# Placing a 6-byte probe on this routine is expected to succeed.
#
# <JmpDirectAt6>:
#   48 85 c9                test   %rcx,%rcx
#   90                      nop
#   90                      nop
#   e9 64 ff ff ff          jmp    <Bar>
#
.text
.global NAME(JmpDirectAt6)
DECLARE_FUNCTION(JmpDirectAt6)

NAME(JmpDirectAt6):
    test %rcx, %rcx
    nop
    nop
    jmp    Bar
    test %rcx, %rcx
    test %rcx, %rcx
    nop # Pad with NOPs to space the functions
    nop
    nop
    nop
    nop
    nop

#
# FUNCTION: CallIndirectReg()
# This function calls Bar() using a indirect CALL instruction.
#
# Placing a 6-byte probe on the CALL instruction is expected to fail.
#
# <CallIndirectReg>:
#  48 8d 0c 25 e0 07 40    lea    <Bar>,%rcx
#  00 
#  90                      nop
#  90                      nop
#  90                      nop
#  ff d1                   call   *%rcx
#  48 85 c9                test   %rcx,%rcx
#  48 85 c9                test   %rcx,%rcx
#  c3                      ret
.text
.global NAME(CallIndirectReg)
DECLARE_FUNCTION(CallIndirectReg)

NAME(CallIndirectReg):
    lea Bar, %rcx
    nop
    nop
    nop
    call *%rcx
    test %rcx, %rcx
    test %rcx, %rcx
    ret
    nop # Pad with NOPs to space the functions
    nop
    nop
    nop
    nop
    nop

#
# FUNCTION: JmpIndirectReg()
# This function calls Bar() using a indirect JMP instruction.
#
# Placing a 6-byte probe on the JMP instruction is expected to succeed only if
# permissive policy is applied that allows placing probes on potential branch targets.
#
# <JmpIndirectReg>:
#  48 8d 0c 25 60 07 40    lea    <Bar>,%rcx
#  00 
#  ff e1                   jmp    *%rcx
#  48 85 c9                test   %rcx,%rcx
#  48 85 c9                test   %rcx,%rcx
#  c3                      ret
.text
.global NAME(JmpIndirectReg)
DECLARE_FUNCTION(JmpIndirectReg)

NAME(JmpIndirectReg):
    lea Bar, %rcx
    nop
    nop
    nop
    jmp *%rcx
    test %rcx, %rcx
    test %rcx, %rcx
    ret
    nop # Pad with NOPs to space the functions
    nop
    nop
    nop
    nop
    nop

#
# FUNCTION: JmpTargetInProbe()
# This function calls Bar() using a direct CALL instruction.
# The first 6 bytes (the size of a probe) overlap with a branch target (L1)
#
# Placing a 6-byte probe on this routine is expected to fail using default probe mode
# and to succeed using a probe mode that allows routine relocation.
#
# <JmpTargetInProbe>:
#           eb 04                   jmp    <L2>
#           90                      nop
# <L1>:     eb 03                   jmp    <L3>
#           90                      nop
# <L2>:     eb fb                   jmp    <L1>
# <L3>:     e8 62 ff ff ff          call   <Bar>
#           c3                      ret
.text
.global NAME(JmpTargetInProbe)
DECLARE_FUNCTION(JmpTargetInProbe)

NAME(JmpTargetInProbe):
    jmp L2
    nop
L1:
    jmp L3
    nop
L2:
    jmp L1
L3:
    call    Bar
    ret

#
# FUNCTION: JmpDirectToNext()
# This function calls Bar() using a direct CALL instruction.
# The probe area includes two branch instructions, JMP and CALL, and a branch target L4
#
# Placing a 6-byte probe on this routine is expected to fail using default probe mode
# and to succeed using a probe mode that allows routine relocation.
#
# <JmpDirectToNext>:
#           eb 00                   jmp    <L4>
# <L4>:     e8 5a ff ff ff          call   <Bar>
#           c3                      ret
.text
.global NAME(JmpDirectToNext)
DECLARE_FUNCTION(JmpDirectToNext)

NAME(JmpDirectToNext):
    jmp L4
L4:
    call    Bar
    ret
