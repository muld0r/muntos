@ vim:ft=arm

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
    ldr r2, =rt_context_prev
    ldr r2, [r2]
    str r1, [r2]

    /* Load the new context returned by rt_syscall_run. */
    ldmia r0!, {r3-r11, lr}
    tst lr, 0x10
    it eq
    vldmiaeq r0!, {s16-s31}

    /* Set the new stack pointer. */
    msr psplim, r3
    msr psp, r0

.Lreturn:
    bx lr
