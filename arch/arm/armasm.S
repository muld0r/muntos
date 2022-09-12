    .syntax unified
    .cpu cortex-m4
    .fpu fpv4-sp-d16
    .thumb

    .section .text.syscall_handler
    .global syscall_handler
    .type syscall_handler, %function
syscall_handler:
    bl rt_self
    push {r4}
    mov r4, r0
    bl rt_syscall_handler
    bl rt_self
    mov r1, r4
    pop {r4}

    cmp r0, r1
    it eq
    bxeq lr

    /* TODO: condtionally save and restore floating-point non-volatile context */

    mrs r2, psp
    stmdb r2!, {r4-r11}
    str r2, [r1]

    ldr r2, [r0]
    ldmia r2!, {r4-r11}
    msr psp, r2

    bx lr

    .size syscall_handler, .-syscall_handler
