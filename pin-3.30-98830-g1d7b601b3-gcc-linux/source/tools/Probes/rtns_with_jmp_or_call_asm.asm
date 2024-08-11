;
; Copyright (C) 2023-2023 Intel Corporation.
; SPDX-License-Identifier: MIT
;

include asm_macros.inc

; The functions in this file call Bar() either using CALL or JMP.
; Each function has different encoding for the JMP/CALL: 
; either direct using offset, direct using a register.
; The JMP/CALL are placed either at the head of the function or
; a few bytes into the function.
; All functions must call Bar(). 
; The purpose of the different variants is to test probing where the JMP/CALL
; overlap with the probe area.


PROLOGUE                          

;
; FUNCTION: Bar()
;
.code
PUBLIC Bar
Bar PROC EXPORT
        BEGIN_STACK_FRAME
        END_STACK_FRAME
        ret
        nop ; Pad with NOPs to space the functions
        nop
        nop
        nop
        nop
        nop
Bar ENDP


;
; FUNCTION: CallDirectAt0()
;
; This function calls Bar() using a direct CALL instruction that is placed
; at offset 0 from the function base.
;
; Placing a 6-byte probe on this routine is expected to fail. (64 bit)
; Placing a 5-byte probe on this routine is expected to succeed. (32 bit)
;
; <CallDirectAt0>: (64 bit)
;  e8 c1 ff ff ff          call   <Bar>
;  c3                      ret
;
; <CallDirectAt0>: (32 bit)
;   e8 fc ff ff ff          call <Bar>
;   c3                      ret
.code
PUBLIC CallDirectAt0
CallDirectAt0 PROC EXPORT
        call Bar
        ret
        nop ; Pad with NOPs to space the functions
        nop
        nop
        nop
        nop
        nop
CallDirectAt0 ENDP

IFDEF TARGET_IA32
;
; FUNCTION: CallDirectAt5()
; This function calls Bar() using a direct CALL instruction that is placed
; at offset 5 from the function base.
;
; Placing a 5-byte probe on this routine is expected to succeed.
;
; <CallDirectAt5>:
;  85 c9                   test   %ecx,%ecx
;  85 c9                   test   %ecx,%ecx
;  e8 fc ff ff ff          call <Bar>
;
.code
PUBLIC CallDirectAt5
CallDirectAt5 PROC EXPORT
        call Bar
        ret
        nop ; Pad with NOPs to space the functions
        nop
        nop
        nop
        nop
        nop
CallDirectAt5 ENDP

ENDIF ; TARGET_IA32


;
; FUNCTION: CallIndirectReg()
; This function calls Bar() using a indirect CALL instruction.
;
; Placing a 6-byte probe on the CALL instruction is expected to fail. (64 bit)
; Placing a 5-byte probe on the CALL instruction is expected to fail. (32 bit)
;
; <CallIndirectReg>: (64 bit)
;  48 8d 0c 25 e0 07 40    lea    <Bar>,%rcx
;  00 
;  90                      nop
;  90                      nop
;  90                      nop
;  ff d1                   call   *%rcx
;  48 85 c9                test   %rcx,%rcx
;  48 85 c9                test   %rcx,%rcx
;  c3                      ret
;
; <CallIndirectReg>: (32 bit)
;  8d 0d 64 86 04 08       lea    <Bar>,%ecx
;  90                      nop
;  90                      nop
;  90                      nop
;  ff d1                   call   *%ecx
;  85 c9                   test   %ecx,%ecx
;  85 c9                   test   %ecx,%ecx
;  c3                      ret
.code
PUBLIC CallIndirectReg
CallIndirectReg PROC EXPORT
        lea GCX_REG, Bar
        nop
        nop
        nop
        call GCX_REG
        test GCX_REG, GCX_REG
        test GCX_REG, GCX_REG
        ret
        nop ; Pad with NOPs to space the functions
        nop
        nop
        nop
        nop
        nop
CallIndirectReg ENDP

;
; FUNCTION: JmpIndirectReg()
; This function calls Bar() using a indirect JMP instruction.
;
; Placing a 6-byte probe (64 bit) or the 5-byte probe (32 bit) on the JMP instruction 
; is expected to succeed only if permissive policy is applied that allows placing probes 
; on potential branch targets.
;
; <JmpIndirectReg>: *64 bit)
;  48 8d 0c 25 60 07 40    lea    <Bar>,%rcx
;  00 
;  ff e1                   jmp    *%rcx
;  48 85 c9                test   %rcx,%rcx
;  48 85 c9                test   %rcx,%rcx
;  c3                      ret
;
; <JmpIndirectReg>: (32 bit)
;   8d 0d 1d 86 04 08       lea    <Bar>,%ecx
;   ff e1                   jmp    *%ecx
;   85 c9                   test   %ecx,%ecx
;   85 c9                   test   %ecx,%ecx
;   c3                      ret
.code
PUBLIC JmpIndirectReg
JmpIndirectReg PROC EXPORT
        lea GCX_REG, Bar
        jmp GCX_REG
        test GCX_REG, GCX_REG
        test GCX_REG, GCX_REG
        ret
        nop ; Pad with NOPs to space the functions
        nop
        nop
        nop
        nop
        nop
JmpIndirectReg ENDP

;
; FUNCTION: JmpDirectAt0()
; This function calls Bar() using a direct JMP instruction that is placed
; at offset 0 from the function base.
;
; Placing a 6-byte probe on this routine is expected to succeed only if
; permissive policy is applied that allows placing probes on potential branch targets. (64 bit)
; Placing a 5-byte probe on this routine is expected to succeed. (32 bit)
;
; <JmpDirectAt0>: (64 bit)
;   e9 7b ff ff ff       	jmp    <Bar>
;   48 85 c9             	test   %rcx,%rcx
;   48 85 c9             	test   %rcx,%rcx
;   c3                   	ret
;
; <CallDirectAt0>: (32 bit)
;   e8 fc ff ff ff          call <Bar>
;   c3                      ret
;
.code
PUBLIC JmpDirectAt0
JmpDirectAt0 PROC EXPORT
        jmp Bar
        test GCX_REG, GCX_REG
        test GCX_REG, GCX_REG
        ret
        nop ; Pad with NOPs to space the functions
        nop
        nop
        nop
        nop
        nop
JmpDirectAt0 ENDP

;
; FUNCTION: JmpDirectAt6()
; This function calls Bar() using a direct JMP instruction that is placed
; at offset 6 from the function base.
;
; Placing a 6-byte probe on this routine is expected to succeed. (64 bit)
;
; <JmpDirectAt6>:
;   48 85 c9                test   %rcx,%rcx
;   90                      nop
;   90                      nop
;   e9 64 ff ff ff          jmp    <Bar>
;
.code
PUBLIC JmpDirectAt6
JmpDirectAt6 PROC EXPORT
        test GCX_REG, GCX_REG
        nop
        nop
        jmp Bar
        ret
        nop ; Pad with NOPs to space the functions
        nop
        nop
        nop
        nop
        nop
JmpDirectAt6 ENDP

; FUNCTION: JmpTargetInProbe()
; This function calls Bar() using a direct CALL instruction.
; The first 6 bytes (the size of a probe) overlap with a branch target (L1)
;
; Placing a 6-byte probe (64 bit) or a 5-byte probe (32 bit) on this routine is expected 
; to fail using default probe mode and to succeed using a probe mode that allows routine relocation.
;
; <JmpTargetInProbe>: (64 bit)
;           eb 04                   jmp    <L2>
;           90                      nop
; <L1>:     eb 03                   jmp    <L3>
;           90                      nop
; <L2>:     eb fb                   jmp    <L1>
; <L3>:     e8 62 ff ff ff          call   <Bar>
;           c3                      ret
;
; <JmpTargetInProbe>: (32 bit)
;           eb 00                   jmp    <L2>
;   <L2>:   eb 00                   jmp    <L3>
;           85 c9                   test   %ecx,%ecx
;           85 c9                   test   %ecx,%ecx
;   <L3>:   e8 bf ff ff ff          call   <Bar>
;           85 c9                   test   %ecx,%ecx
;           85 c9                   test   %ecx,%ecx
;           c3                      ret
;
.code
PUBLIC JmpTargetInProbe
JmpTargetInProbe PROC EXPORT
        jmp L2
        nop
L1:
        jmp L3
        nop
L2:
        jmp L1
L3:
        call Bar
        ret
        nop ; Pad with NOPs to space the functions
        nop
        nop
        nop
        nop
        nop
JmpTargetInProbe ENDP

; FUNCTION: JmpDirectToNext()
; This function calls Bar() using a direct CALL instruction.
; The probe area includes two branch instructions, JMP and CALL, and a branch target L4
;
; Placing a 6-byte probe (64 bit) or a 5-byte probe (32 bit) on this routine is expected 
; to fail using default probe mode and to succeed using a probe mode that allows routine relocation.
;
; <JmpDirectToNext>: (64 bit)
;           eb 00                   jmp    <L4>
; <L4>:     e8 5a ff ff ff          call   <Bar>
;           c3                      ret
;
; <JmpDirectToNext>: (32 bit)
;           eb 00                   jmp    <L4>
; <L4>:     e8 5a ff ff ff          call   <Bar>
;           c3                      ret
.code
PUBLIC JmpDirectToNext
JmpDirectToNext PROC EXPORT
        jmp L4
L4:
        call Bar
        ret
        nop ; Pad with NOPs to space the functions
        nop
        nop
        nop
        nop
        nop
JmpDirectToNext ENDP

END ; end PROLOGUE
