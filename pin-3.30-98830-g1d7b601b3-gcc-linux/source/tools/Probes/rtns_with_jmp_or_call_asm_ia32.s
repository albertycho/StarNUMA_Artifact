/*
 * Copyright (C) 2023-2023 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

#include <asm_macros.h>

# The functions in this file call Bar() either using CALL or JMP.
# Each function has different encoding for the JMP/CALL: 
# either direct using offset, direct using a register.
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
    push   %ebp
    mov    %esp, %ebp
    leave
    ret


#
# FUNCTION: CallDirectAt0()
# This function calls Bar() using a direct CALL instruction that is placed
# at offset 0 from the function base.
#
# Placing a 5-byte probe on this routine is expected to succeed.
#
# <CallDirectAt0>:
#   e8 fc ff ff ff          call <Bar>
#   c3                      ret
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
# FUNCTION: CallDirectAt5()
# This function calls Bar() using a direct CALL instruction that is placed
# at offset 5 from the function base.
#
# Placing a 5-byte probe on this routine is expected to succeed.
#
# <CallDirectAt5>:
#  85 c9                   test   %ecx,%ecx
#  85 c9                   test   %ecx,%ecx
#  e8 fc ff ff ff          call <Bar>
#  c3                      ret
#
.text
.global NAME(CallDirectAt5)
DECLARE_FUNCTION(CallDirectAt5)

NAME(CallDirectAt5):
    test %ecx, %ecx
    test %ecx, %ecx
    call    Bar
    ret
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
# Placing a 5-byte probe on the CALL instruction is expected to fail.
#
# <CallIndirectReg>:
#  8d 0d 64 86 04 08       lea    <Bar>,%ecx
#  90                      nop
#  90                      nop
#  90                      nop
#  ff d1                   call   *%ecx
#  85 c9                   test   %ecx,%ecx
#  85 c9                   test   %ecx,%ecx
#  c3                      ret
#
.text
.global NAME(CallIndirectReg)
DECLARE_FUNCTION(CallIndirectReg)

NAME(CallIndirectReg):
    lea Bar, %ecx
    nop
    nop
    nop
    calll *%ecx
    test %ecx, %ecx
    test %ecx, %ecx
    ret
    nop # Pad with NOPs to space the functions
    nop
    nop
    nop
    nop
    nop

#
# FUNCTION: JmpTargetInProbe()
# This function calls Bar() using a direct JMP instruction.
# The first 5 bytes (the size of a probe) overlap with a branch targets (L2 and L3)
#
# Placing a 5-byte probe on this routine is expected to fail using default probe mode
# and to succeed using a probe mode that allows routine relocation.
#
# <JmpTargetInProbe>:
#           eb 00                   jmp    <L2>
#   <L2>:   eb 00                   jmp    <L3>
#           85 c9                   test   %ecx,%ecx
#           85 c9                   test   %ecx,%ecx
#   <L3>:   e8 bf ff ff ff          call   <Bar>
#           85 c9                   test   %ecx,%ecx
#           85 c9                   test   %ecx,%ecx
#           c3                      ret
#
.text
.global NAME(JmpTargetInProbe)
DECLARE_FUNCTION(JmpTargetInProbe)

NAME(JmpTargetInProbe):
L1:
    jmp L2
L2:
    jmp L3
L3:
    test %ecx, %ecx
    test %ecx, %ecx
    call Bar
    test %ecx, %ecx
    test %ecx, %ecx
    ret
    nop # Pad with NOPs to space the functions
    nop
    nop
    nop
    nop
    nop

#
# FUNCTION: JmpDirectToNext()
# This function calls Bar() using a direct CALL instruction.
# The probe area includes two branch instructions, JMP and CALL, and a branch target L4
#
# Placing a 5-byte probe on this routine is expected to fail using default probe mode
# and to succeed using a probe mode that allows routine relocation.
#
# <JmpDirectToNext>:
#       eb 00                	jmp    <L4>
# <L4>: e8 ad ff ff ff       	call   <Bar>
#       c3                   	ret
.text
.global NAME(JmpDirectToNext)
DECLARE_FUNCTION(JmpDirectToNext)

NAME(JmpDirectToNext):
    jmp L4
L4:
    call    Bar
    ret

#
# FUNCTION: JmpIndirectReg()
# This function calls Bar() using a indirect JMP instruction.
#
# Placing a 5-byte probe on the JMP instruction is expected to succeed.
#
# <JmpIndirectReg>:
#   8d 0d 1d 86 04 08       lea    <Bar>,%ecx
#   ff e1                   jmp    *%ecx
#   85 c9                   test   %ecx,%ecx
#   85 c9                   test   %ecx,%ecx
#   c3                      ret
.text
.global NAME(JmpIndirectReg)
DECLARE_FUNCTION(JmpIndirectReg)

NAME(JmpIndirectReg):
    lea Bar, %ecx
    jmpl *%ecx
    test %ecx, %ecx
    test %ecx, %ecx
    ret
    nop # Pad with NOPs to space the functions
    nop
    nop
    nop
    nop
    nop

