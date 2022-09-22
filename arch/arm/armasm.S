    .syntax unified
    .cpu cortex-m4
    .fpu fpv4-sp-d16
    .thumb

    .section .text.syscall_handler
    .global syscall_handler
    .type syscall_handler, %function
syscall_handler:
    bl rt_syscall_run
    cmp r0, 0
    it eq
    bxeq lr

    /* TODO: condtionally save and restore floating-point non-volatile context */

    ldr r1, =rt_prev_task

    mrs r2, psp
    stmdb r2!, {r4-r11, lr}
    str r2, [r1]

    ldmia r0!, {r4-r11, lr}
    msr psp, r0

    bx lr

    .size syscall_handler, .-syscall_handler
