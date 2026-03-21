    .section ".entry.text", "ax"

    .global default_vector_table

    .balign 0x800
default_vector_table:
// Current EL with SP0
    .balign 0x80
curr_el_sp0_sync:       // Synchronous
    b undefined_handler

    .balign 0x80
curr_el_sp0_irq:        // IRQ/vIRQ
    b undefined_handler

    .balign 0x80
curr_el_sp0_fiq:        // FIQ/vFIQ
    b undefined_handler

    .balign 0x80
curr_el_sp0_serr:       // SError/vSError
    b undefined_handler

// Current EL with SPx
    .balign 0x80
curr_el_spx_sync:       // Synchronous
    b undefined_handler

    .balign 0x80
curr_el_spx_irq:        // IRQ/vIRQ
    b timer_handler

    .balign 0x80
curr_el_spx_fiq:        // FIQ/vFIQ
    b undefined_handler

    .balign 0x80
curr_el_spx_serr:       // SError/vSError
    b undefined_handler

// Lower EL using AArch64
    .balign 0x80
lower_el_a64_sync:       // Synchronous
    b undefined_handler

    .balign 0x80
lower_el_a64_irq:        // IRQ/vIRQ
    b undefined_handler

    .balign 0x80
lower_el_a64_fiq:        // FIQ/vFIQ
    b undefined_handler

    .balign 0x80
lower_el_a64_serr:       // SError/vSError
    b undefined_handler

// Lower EL using AArch32
    .balign 0x80
lower_el_a32_sync:       // Synchronous
    b undefined_handler

    .balign 0x80
lower_el_a32_irq:        // IRQ/vIRQ
    b undefined_handler

    .balign 0x80
lower_el_a32_fiq:        // FIQ/vFIQ
    b undefined_handler

    .balign 0x80
lower_el_a32_serr:       // SError/vSError
    b undefined_handler

    .balign 0x80
undefined_handler:
    eret


.global timer_handler
	.balign 0x80
timer_handler:
// まず x18, x30（LR）を保存
    stp x18, x30, [sp, #-16]!

    // caller-saved を保存
    stp x0,  x1,  [sp, #-16]!
    stp x2,  x3,  [sp, #-16]!
    stp x4,  x5,  [sp, #-16]!
    stp x6,  x7,  [sp, #-16]!
    stp x8,  x9,  [sp, #-16]!
    stp x10, x11, [sp, #-16]!
    stp x12, x13, [sp, #-16]!
    stp x14, x15, [sp, #-16]!
    stp x16, x17, [sp, #-16]!

    bl  InterruptHandler

    // 逆順で復元
    ldp x16, x17, [sp], #16
    ldp x14, x15, [sp], #16
    ldp x12, x13, [sp], #16
    ldp x10, x11, [sp], #16
    ldp x8,  x9,  [sp], #16
    ldp x6,  x7,  [sp], #16
    ldp x4,  x5,  [sp], #16
    ldp x2,  x3,  [sp], #16
    ldp x0,  x1,  [sp], #16

    // 最後に x18, x30 を復元
    ldp x18, x30, [sp], #16
    eret