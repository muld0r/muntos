    .syntax unified
    .arch armv7-m
    .thumb

    .section .text.syscall_handler
    .global syscall_handler
    .type syscall_handler, %function
syscall_handler:
    push {lr}
    bl rt_syscall_run
    pop {lr}
    cmp r0, 0
    it eq
    bxeq lr

    mrs r1, psp
#if defined(__ARM_FP)
    tst lr, 0x10
    it eq
    vstmdbeq r1!, {s16-s31}
#endif
    stmdb r1!, {r4-r11, lr}
    ldr r2, =rt_prev_task
    ldr r2, [r2]
    str r1, [r2]

    ldmia r0!, {r4-r11, lr}
#if defined(__ARM_FP)
    tst lr, 0x10
    it eq
    vldmiaeq r0!, {s16-s31}
#endif
    msr psp, r0

    bx lr
