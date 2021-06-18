/*
            Copyright Oliver Kowalke 2009.
   Distributed under the Boost Software License, Version 1.0.
      (See accompanying file LICENSE_1_0.txt or copy at
            http://www.boost.org/LICENSE_1_0.txt)
*/

/****************************************************************************************
 *                                                                                      *
 *  ----------------------------------------------------------------------------------  *
 *  |    0    |    1    |    2    |    3    |    4     |    5    |    6    |    7    |  *
 *  ----------------------------------------------------------------------------------  *
 *  |   0x0   |   0x4   |   0x8   |   0xc   |   0x10   |   0x14  |   0x18  |   0x1c  |  *
 *  ----------------------------------------------------------------------------------  *
 *  | fc_mxcsr|fc_x87_cw|        R12        |         R13        |        R14        |  *
 *  ----------------------------------------------------------------------------------  *
 *  ----------------------------------------------------------------------------------  *
 *  |    8    |    9    |   10    |   11    |    12    |    13   |    14   |    15   |  *
 *  ----------------------------------------------------------------------------------  *
 *  |   0x20  |   0x24  |   0x28  |  0x2c   |   0x30   |   0x34  |   0x38  |   0x3c  |  *
 *  ----------------------------------------------------------------------------------  *
 *  |        R15        |        RBX        |         RBP        |        RIP        |  *
 *  ----------------------------------------------------------------------------------  *
 *  ----------------------------------------------------------------------------------  *
 *  |    16   |   17    |                                                            |  *
 *  ----------------------------------------------------------------------------------  *
 *  |   0x40  |   0x44  |                                                            |  *
 *  ----------------------------------------------------------------------------------  *
 *  |        EXIT       |                                                            |  *
 *  ----------------------------------------------------------------------------------  *
 *                                                                                      *
 ****************************************************************************************/

#include "abt_config.h"

.text
.globl make_fcontext
.type make_fcontext,@function
.align 16
make_fcontext:
    /* first arg of make_fcontext() == top of context-stack */
    movq  %rdi, %rax

    /* shift address in RAX to lower 16 byte boundary */
    andq  $-16, %rax

    /* reserve space for context-data on context-stack */
    /* size for fc_mxcsr .. RIP + return-address for context-function */
    /* on context-function entry: (RSP -0x8) % 16 == 0 */
    leaq  -0x48(%rax), %rax

    /* third arg of make_fcontext() == address of context-function */
    movq  %rdx, 0x38(%rax)

    /* save MMX control- and status-word */
    stmxcsr  (%rax)
    /* save x87 control-word */
    fnstcw   0x4(%rax)

    /* compute abs address of label finish */
    leaq  finish(%rip), %rcx
    /* save address of finish as return-address for context-function */
    /* will be entered after context-function returns */
    movq  %rcx, 0x40(%rax)

    ret /* return pointer to context-data */

finish:
    /* exit code is zero */
    xorq  %rdi, %rdi
    /* exit application */
    call  _exit@PLT
    hlt
.size make_fcontext,.-make_fcontext

.text
.globl jump_fcontext
.type jump_fcontext,@function
.align 16
jump_fcontext:
    pushq  %rbp  /* save RBP */
    pushq  %rbx  /* save RBX */
    pushq  %r15  /* save R15 */
    pushq  %r14  /* save R14 */
    pushq  %r13  /* save R13 */
    pushq  %r12  /* save R12 */

    /* prepare stack for FPU */
    leaq  -0x8(%rsp), %rsp

#if ABTD_FCONTEXT_PRESERVE_FPU
    /* save MMX control- and status-word */
    stmxcsr  (%rsp)
    /* save x87 control-word */
    fnstcw   0x4(%rsp)
#endif

    /* store RSP (pointing to context-data) in RDI */
    movq  %rsp, (%rdi)

    /* restore RSP (pointing to context-data) from RSI */
    movq  %rsi, %rsp

#if ABTD_FCONTEXT_PRESERVE_FPU
    /* restore MMX control- and status-word */
    ldmxcsr  (%rsp)
    /* restore x87 control-word */
    fldcw  0x4(%rsp)
#endif

    /* prepare stack for FPU */
    leaq  0x8(%rsp), %rsp

    popq  %r12  /* restrore R12 */
    popq  %r13  /* restrore R13 */
    popq  %r14  /* restrore R14 */
    popq  %r15  /* restrore R15 */
    popq  %rbx  /* restrore RBX */
    popq  %rbp  /* restrore RBP */

    /* restore return-address */
    popq  %r8

    /* use third arg as return-value after jump */
    /* movq  %rdx, %rax */
    /* use third arg as first arg in context function */
    movq  %rdx, %rdi

    /* indirect jump to context */
    jmp  *%r8
.size jump_fcontext,.-jump_fcontext

.text
.globl peek_fcontext
.type peek_fcontext,@function
.align 16
peek_fcontext:
    /* temporarily move RSP (pointing to context-data) to R12 (callee-saved) */
    pushq  %r12
    movq   %rsp, %r12
    /* restore RSP (pointing to context-data) from RDI */
    movq   %rdi, %rsp
    /* RSP is already 16-byte aligned, so we can call a peek function here
     * rsi(rdx) => second_arg(third_arg) */
    movq   %rdx, %rdi
    callq *%rsi
    /* restore callee-saved registers. */
    movq   %r12, %rsp
    popq   %r12
    ret
.size peek_fcontext,.-peek_fcontext

.text
.globl take_fcontext
.type take_fcontext,@function
.align 16
take_fcontext:
    /* restore RSP (pointing to context-data) from RSI */
    movq  %rsi, %rsp

#if ABTD_FCONTEXT_PRESERVE_FPU
    /* restore MMX control- and status-word */
    ldmxcsr  (%rsp)
    /* restore x87 control-word */
    fldcw  0x4(%rsp)
#endif

    /* prepare stack for FPU */
    leaq  0x8(%rsp), %rsp

    popq  %r12  /* restrore R12 */
    popq  %r13  /* restrore R13 */
    popq  %r14  /* restrore R14 */
    popq  %r15  /* restrore R15 */
    popq  %rbx  /* restrore RBX */
    popq  %rbp  /* restrore RBP */

    /* restore return-address */
    popq  %r8

    /* use third arg as return-value after jump */
    /* movq  %rdx, %rax */
    /* use third arg as first arg in context function */
    movq  %rdx, %rdi

    /* indirect jump to context */
    jmp  *%r8
.size take_fcontext,.-take_fcontext


/* Mark that we don't need executable stack.  */
#ifndef __SUNPRO_C
.section .note.GNU-stack,"",%progbits
#else
/* Solaris assembler does not recognize it.  Let's ignore so far. */
#endif
