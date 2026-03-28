#include "armv8def.h"

    .section .reset,"ax",@progbits
    .globl _start
_start:
    /* Turn into EL1 with the EL1 stack */
    mov x3, #(HCR_EL2_RW)
    msr HCR_EL2, x3
    isb
    ldr x0, =EL1_start
    msr ELR_EL2, x0
    mov x0, PSR_A_BIT | PSR_I_BIT | PSR_F_BIT | PSR_MODE64_BIT | PSR_MODE_EL1h
    msr    SPSR_EL2, x0

    eret

EL1_start:
    ldr x4, =_stack_end
    mov sp, x4

    bl main
    b .