/*******************************************************
 *                                                     *
 *  -------------------------------------------------  *
 *  |  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  *
 *  -------------------------------------------------  *
 *  | 0x0 | 0x4 | 0x8 | 0xc | 0x10| 0x14| 0x18| 0x1c|  *
 *  -------------------------------------------------  *
 *  |    d8     |    d9     |    d10    |    d11    |  *
 *  -------------------------------------------------  *
 *  -------------------------------------------------  *
 *  |  8  |  9  |  10 |  11 |  12 |  13 |  14 |  15 |  *
 *  -------------------------------------------------  *
 *  | 0x20| 0x24| 0x28| 0x2c| 0x30| 0x34| 0x38| 0x3c|  *
 *  -------------------------------------------------  *
 *  |    d12    |    d13    |    d14    |    d15    |  *
 *  -------------------------------------------------  *
 *  -------------------------------------------------  *
 *  |  16 |  17 |  18 |  19 |  20 |  21 |  22 |  23 |  *
 *  -------------------------------------------------  *
 *  | 0x40| 0x44| 0x48| 0x4c| 0x50| 0x54| 0x58| 0x5c|  *
 *  -------------------------------------------------  *
 *  |    x19    |    x20    |    x21    |    x22    |  *
 *  -------------------------------------------------  *
 *  -------------------------------------------------  *
 *  |  24 |  25 |  26 |  27 |  28 |  29 |  30 |  31 |  *
 *  -------------------------------------------------  *
 *  | 0x60| 0x64| 0x68| 0x6c| 0x70| 0x74| 0x78| 0x7c|  *
 *  -------------------------------------------------  *
 *  |    x23    |    x24    |    x25    |    x26    |  *
 *  -------------------------------------------------  *
 *  -------------------------------------------------  *
 *  |  32 |  33 |  34 |  35 |  36 |  37 |  38 |  39 |  *
 *  -------------------------------------------------  *
 *  | 0x80| 0x84| 0x88| 0x8c| 0x90| 0x94| 0x98| 0x9c|  *
 *  -------------------------------------------------  *
 *  |    x27    |    x28    |    FP     |     LR    |  *
 *  -------------------------------------------------  *
 *  -------------------------------------------------  *
 *  |  40 |  41 |  42 | 43  |           |           |  *
 *  -------------------------------------------------  *
 *  | 0xa0| 0xa4| 0xa8| 0xac|           |           |  *
 *  -------------------------------------------------  *
 *  |     PC    |   align   |           |           |  *
 *  -------------------------------------------------  *
 *                                                     *
 *******************************************************/

#include "abt_config.h"

.text
.globl _make_fcontext
.balign 16

_make_fcontext:
    ; shift address in x0 (allocated stack) to lower 16 byte boundary
    and x0, x0, ~0xF

    ; reserve space for context-data on context-stack
    sub  x0, x0, #0xb0

    ; third arg of make_fcontext() == address of context-function
    ; store address as a PC to jump in
    str  x2, [x0, #0xa0]

    ; compute abs address of label finish
    ; 0x0c = 3 instructions * size (4) before label 'finish'

    ; TODO: Numeric offset since llvm still does not support labels in ADR. Fix:
    ;       http://lists.cs.uiuc.edu/pipermail/llvm-commits/Week-of-Mon-20140407/212336.html
    adr  x1, 0x0c

    ; save address of finish as return-address for context-function
    ; will be entered after context-function returns (LR register)
    str  x1, [x0, #0x98]

    ret  lr ; return pointer to context-data (x0)

finish:
    ; exit code is zero
    mov  x0, #0
    ; exit application
    bl  __exit


.text
.globl _jump_fcontext
.balign 16
_jump_fcontext:
    ; prepare stack for GP + FPU
    sub  sp, sp, #0xb0

#if ABTD_FCONTEXT_PRESERVE_FPU
    ; save d8 - d15
    stp  d8,  d9,  [sp, #0x00]
    stp  d10, d11, [sp, #0x10]
    stp  d12, d13, [sp, #0x20]
    stp  d14, d15, [sp, #0x30]
#endif

    ; save x19-x30
    stp  x19, x20, [sp, #0x40]
    stp  x21, x22, [sp, #0x50]
    stp  x23, x24, [sp, #0x60]
    stp  x25, x26, [sp, #0x70]
    stp  x27, x28, [sp, #0x80]
    stp  fp,  lr,  [sp, #0x90]

    ; save LR as PC
    str  lr, [sp, #0xa0]

    ; store RSP (pointing to context-data) in first argument (x0).
    ; STR cannot have sp as a target register
    mov  x4, sp
    str  x4, [x0]

    ; restore RSP (pointing to context-data) from A2 (x1)
    mov  sp, x1

#if ABTD_FCONTEXT_PRESERVE_FPU
    ; load d8 - d15
    ldp  d8,  d9,  [sp, #0x00]
    ldp  d10, d11, [sp, #0x10]
    ldp  d12, d13, [sp, #0x20]
    ldp  d14, d15, [sp, #0x30]
#endif

    ; load x19-x30
    ldp  x19, x20, [sp, #0x40]
    ldp  x21, x22, [sp, #0x50]
    ldp  x23, x24, [sp, #0x60]
    ldp  x25, x26, [sp, #0x70]
    ldp  x27, x28, [sp, #0x80]
    ldp  fp,  lr,  [sp, #0x90]

    ; use third arg as return value after jump
    ; and as first arg in context function
    mov  x0, x2

    ; load pc
    ldr  x4, [sp, #0xa0]

    ; restore stack from GP + FPU
    add  sp, sp, #0xb0

    ret x4


.text
.globl _take_fcontext
.balign 16
_take_fcontext:
    ; restore RSP (pointing to context-data) from A2 (x1)
    mov  sp, x1

#if ABTD_FCONTEXT_PRESERVE_FPU
    ; load d8 - d15
    ldp  d8,  d9,  [sp, #0x00]
    ldp  d10, d11, [sp, #0x10]
    ldp  d12, d13, [sp, #0x20]
    ldp  d14, d15, [sp, #0x30]
#endif

    ; load x19-x30
    ldp  x19, x20, [sp, #0x40]
    ldp  x21, x22, [sp, #0x50]
    ldp  x23, x24, [sp, #0x60]
    ldp  x25, x26, [sp, #0x70]
    ldp  x27, x28, [sp, #0x80]
    ldp  fp,  lr,  [sp, #0x90]

    ; use third arg as return value after jump
    ; and as first arg in context function
    mov  x0, x2

    ; load pc
    ldr  x4, [sp, #0xa0]

    ; restore stack from GP + FPU
    add  sp, sp, #0xb0

    ret x4

