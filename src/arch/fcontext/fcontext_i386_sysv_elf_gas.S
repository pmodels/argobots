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
 *  | fc_mxcsr|fc_x87_cw|   EDI   |   ESI   |   EBX    |   EBP   |   EIP   |   EXIT  |  *
 *  ----------------------------------------------------------------------------------  *
 *                                                                                      *
 ****************************************************************************************/

#include "abt_config.h"

.text
.globl make_fcontext
.align 2
.type make_fcontext,@function
make_fcontext:
    /* first arg of make_fcontext() == top of context-stack */
    movl  0x4(%esp), %eax

    /* reserve space for first argument of context-function
       rax might already point to a 16byte border */
    leal  -0x8(%eax), %eax

    /* shift address in EAX to lower 16 byte boundary */
    andl  $-16, %eax

    /* reserve space for context-data on context-stack */
    /* size for fc_mxcsr .. EIP + return-address for context-function */
    /* on context-function entry: (ESP -0x4) % 8 == 0 */
    leal  -0x20(%eax), %eax

    /* third arg of make_fcontext() == address of context-function */
    movl  0xc(%esp), %edx
    movl  %edx, 0x18(%eax)

    /* save MMX control- and status-word */
    stmxcsr  (%eax)
    /* save x87 control-word */
    fnstcw  0x4(%eax)

    /* compute abs address of label finish */
    call  1f
    /* address of label 1 */
1:  popl  %ecx
    /* compute abs address of label finish */
    addl  $finish-1b, %ecx
    /* save address of finish as return-address for context-function */
    /* will be entered after context-function returns */
    movl  %ecx, 0x1c(%eax)

    ret /* return pointer to context-data */

finish:
    call  2f
    /* address of label 2 */
2:  popl  %ebx
    /* compute address of GOT and store it in EBX */
    addl  $_GLOBAL_OFFSET_TABLE_+[.-2b], %ebx

    /* exit code is zero */
    xorl  %eax, %eax
    movl  %eax, (%esp)
    /* exit application */
    call  _exit@PLT
    hlt
.size make_fcontext,.-make_fcontext


.text
.globl jump_fcontext
.align 2
.type jump_fcontext,@function
jump_fcontext:
    pushl  %ebp  /* save EBP */
    pushl  %ebx  /* save EBX */
    pushl  %esi  /* save ESI */
    pushl  %edi  /* save EDI */

    /* prepare stack for FPU */
    leal  -0x8(%esp), %esp

#if ABTD_FCONTEXT_PRESERVE_FPU
    /* save MMX control- and status-word */
    stmxcsr  (%esp)
    /* save x87 control-word */
    fnstcw  0x4(%esp)
#endif

    /* first arg of jump_fcontext() == context jumping from */
    movl  0x1c(%esp), %eax

    /* store ESP (pointing to context-data) in EAX */
    movl  %esp, (%eax)

    /* second arg of jump_fcontext() == context jumping to */
    movl  0x20(%esp), %edx

    /* third arg of jump_fcontext() == value to be returned after jump */
    movl  0x24(%esp), %eax

    /* restore ESP (pointing to context-data) from EDX */
    movl  %edx, %esp

#if ABTD_FCONTEXT_PRESERVE_FPU
    /* restore MMX control- and status-word */
    ldmxcsr  (%esp)
    /* restore x87 control-word */
    fldcw  0x4(%esp)
#endif

    /* prepare stack for FPU */
    leal  0x8(%esp), %esp

    popl  %edi  /* restore EDI */
    popl  %esi  /* restore ESI */
    popl  %ebx  /* restore EBX */
    popl  %ebp  /* restore EBP */

    /* restore return-address */
    popl  %edx

    /* use value in EAX (originally third arg) as
     * 1. return value after jump or
     * 2. first arg in context function. */
    movl  %eax, 0x4(%esp)

    /* indirect jump to context */
    jmp  *%edx
.size jump_fcontext,.-jump_fcontext


.text
.globl take_fcontext
.align 2
.type take_fcontext,@function
take_fcontext:
    /* first arg is ignored. */

    /* second arg of take_fcontext() == context jumping to */
    movl  0x8(%esp), %edx

    /* third arg of take_fcontext() == value to be returned after jump */
    movl  0xc(%esp), %eax

    /* restore ESP (pointing to context-data) from EDX */
    movl  %edx, %esp

#if ABTD_FCONTEXT_PRESERVE_FPU
    /* restore MMX control- and status-word */
    ldmxcsr  (%esp)
    /* restore x87 control-word */
    fldcw  0x4(%esp)
#endif

    /* prepare stack for FPU */
    leal  0x8(%esp), %esp

    popl  %edi  /* restore EDI */
    popl  %esi  /* restore ESI */
    popl  %ebx  /* restore EBX */
    popl  %ebp  /* restore EBP */

    /* restore return-address */
    popl  %edx

    /* use value in EAX (originally third arg) as
     * 1. return value after jump or
     * 2. first arg in context function. */
    movl  %eax, 0x4(%esp)

    /* indirect jump to context */
    jmp  *%edx
.size take_fcontext,.-take_fcontext


/* Mark that we don't need executable stack.  */
.section .note.GNU-stack,"",%progbits
