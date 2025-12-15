
    .section .reset,"ax",@progbits
    .globl _start
_start:
    ldr x4, =_stack_end
    mov sp, x4

    bl main
    b .

