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

    mrs r1, psp
    tst lr, 0x10
    it eq
    vstmdbeq r1!, {s16-s31}
    stmdb r1!, {r4-r11, lr}
    ldr r2, =rt_prev_task
    str r1, [r2]

    ldmia r0!, {r4-r11, lr}
    tst lr, 0x10
    it eq
    vldmiaeq r0!, {s16-s31}
    msr psp, r0
    isb

    bx lr

    .size syscall_handler, .-syscall_handler
