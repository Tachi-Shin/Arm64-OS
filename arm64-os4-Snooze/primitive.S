#include "armv8def.h"

    .global EnableInt
    .type EnableInt,@function
EnableInt:
    MSR   DAIFClr,  #(DAIF_A | DAIF_F | DAIF_I)
    ret
    .size EnableInt,.-EnableInt

    .global DisableInt
    .type DisableInt,@function
DisableInt:
    MSR   DAIFSet,  #(DAIF_A | DAIF_F | DAIF_I)
    ret
    .size DisableInt,.-DisableInt

    .globl GetCoreID
    .type GetCoreID,@function
    .balign 4
GetCoreID:
    mrs x0, MPIDR_EL1
    ubfx    x0, x0, #0, #8              // coreid == affinity0
    ret
    .size GetCoreID,.-GetCoreID

    .globl switch_context
    .globl load_context

// void switch_context(unsigned long *next_sp, unsigned long *cur_sp)
switch_context:
    // 保存: Callee-Saved Regs + LR
    stp     x19, x20, [sp, #-16]!
    stp     x21, x22, [sp, #-16]!
    stp     x23, x24, [sp, #-16]!
    stp     x25, x26, [sp, #-16]!
    stp     x27, x28, [sp, #-16]!
    stp     x29, x18, [sp, #-16]!
    stp     x30, x17, [sp, #-16]!

    // workaround: str sp, [x1] → mov x9, sp → str x9, [x1]
    mov     x9, sp
    str     x9, [x1]            // *cur_sp = current stack pointer

    // 次のSPをload_contextに渡す
    b       load_context

// void load_context(unsigned long *next_sp)
load_context:
    ldr     x9, [x0]            // x9 = *next_sp
    mov     sp, x9              // sp = x9

    // 復元（逆順）
    ldp     x30, x17, [sp], #16
    ldp     x29, x18, [sp], #16
    ldp     x27, x28, [sp], #16
    ldp     x25, x26, [sp], #16
    ldp     x23, x24, [sp], #16
    ldp     x21, x22, [sp], #16
    ldp     x19, x20, [sp], #16

    ret
