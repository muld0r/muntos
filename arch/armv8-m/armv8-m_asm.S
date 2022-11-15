    .syntax unified
    .thumb
    .p2align 1
    .section .text.rt_syscall_handler
    .global rt_syscall_handler
    .type rt_syscall_handler, %function
rt_syscall_handler:

#if defined(__ARM_FP)

    push {r3, lr}
    bl rt_syscall_run
    pop {r3, lr}
    /* If there's no new context to switch to, return early. */
    cbz r0, .Lreturn

    /* Write the suspending context to its stack pointer. */
    mrs r1, psp
    mrs r3, psplim
    tst lr, 0x10
    it eq
    vstmdbeq r1!, {s16-s31}
    stmdb r1!, {r3-r11, lr}

    /* Store the new stack pointer with the saved context. */
    ldr r2, =rt_prev_context
    ldr r2, [r2]
    str r1, [r2]

    /* Load the new register context returned by rt_syscall_run. */
    ldmia r0!, {r3-r11, lr}
    tst lr, 0x10
    it eq
    vldmiaeq r0!, {s16-s31}

    /* Set the new stack pointer. */
    msr psp, r0
    msr psplim, r3

.Lreturn:
    bx lr

#else /* no FP */

    bl rt_syscall_run
    cbz r0, .Lreturn

    mrs r1, psp
    mrs r3, psplim
    stmdb r1!, {r3-r11}

    ldr r2, =rt_prev_context
    ldr r2, [r2]
    str r1, [r2]

    ldmia r0!, {r3-r11}
    msr psp, r0
    msr psplim, r3

.Lreturn:
    /* The thread exception return value is always -68 == 0xFFFFFFBC. */
    mov r0, -68
    bx r0

#endif

    .size rt_syscall_handler, .-rt_syscall_handler
