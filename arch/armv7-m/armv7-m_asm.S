    .syntax unified
    .arch armv7-m
    .thumb
    .p2align 1

    .section .text.rt_syscall_handler
    .global rt_syscall_handler
    .type rt_syscall_handler, %function
rt_syscall_handler:
    push {lr}
    bl rt_syscall_run
    pop {lr}
    /* If there's no new context to switch to, return early. */
    cbz r0, .Lreturn

    /* Write the suspending context to its stack pointer. */
    mrs r1, psp
#if defined(__ARM_FP)
    tst lr, 0x10
    it eq
    vstmdbeq r1!, {s16-s31}
    stmdb r1!, {r4-r11, lr}
#else
    /* If FP is not used, we use the same exception return lr for all tasks. */
    stmdb r1!, {r4-r11}
#endif

    /* Store the new stack pointer with the saved context. */
    ldr r2, =rt_prev_context
    ldr r2, [r2]
    str r1, [r2]

    /* Load the new register context returned by rt_syscall_run. */
#if defined(__ARM_FP)
    ldmia r0!, {r4-r11, lr}
    tst lr, 0x10
    it eq
    vldmiaeq r0!, {s16-s31}
#else
    ldmia r0!, {r4-r11}
#endif

    /* Set the new stack pointer. */
    msr psp, r0

.Lreturn:
    bx lr
