    .syntax unified
    .arch armv7-m
    .thumb

    .section .text.rt_syscall_handler
    .global rt_syscall_handler
    .type rt_syscall_handler, %function
rt_syscall_handler:
    push {lr}
    bl rt_syscall_run
    pop {lr}
    /* If there's no new context to swap to, return early. */
    cmp r0, 0
    it eq
    bxeq lr

    /* Write the suspending context to its stack pointer. */
    mrs r1, psp
#if defined(__ARM_FP)
    tst lr, 0x10
    it eq
    vstmdbeq r1!, {s16-s31}
#endif
    stmdb r1!, {r4-r11, lr}
    /* Store the new stack pointer with the saved context in the suspending task. */
    ldr r2, =rt_prev_task
    ldr r2, [r2]
    str r1, [r2]

    /* Load the new register context returned by rt_syscall_run. */
    ldmia r0!, {r4-r11, lr}
#if defined(__ARM_FP)
    tst lr, 0x10
    it eq
    vldmiaeq r0!, {s16-s31}
#endif
    /* Set the new stack pointer. */
    msr psp, r0

    bx lr
