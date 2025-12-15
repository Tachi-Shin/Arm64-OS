    .globl switch_context
    .globl load_context
    .type switch_context,@function
    .balign 4
switch_context:
    stp     x29, x30, [sp, #-0x10]!
    stp     x27, x28, [sp, #-0x10]!
    stp     x25, x26, [sp, #-0x10]!
    stp     x23, x24, [sp, #-0x10]!
    stp     x21, x22, [sp, #-0x10]!
    stp     x19, x20, [sp, #-0x10]!
    mov     x2, sp
    str     x2, [x1]

load_context:
    ldr     x3, [x0]
    mov     sp, x3
    ldp     x19, x20, [sp], #0x10
    ldp     x21, x22, [sp], #0x10
    ldp     x23, x24, [sp], #0x10
    ldp     x25, x26, [sp], #0x10
    ldp     x27, x28, [sp], #0x10
    ldp     x29, x30, [sp], #0x10
    ret
    .size switch_context,.-switch_context