@ vim:ft=arm

#if __ARM_ARCH == 8
#define base r3
#define getpsplim mrs r3, psplim
#define setpsplim msr psplim, r3
#else /* v7 */
#define base r4
#define getpsplim
#define setpsplim
#endif

    push {r3, lr}
    bl rt_syscall_run
    pop {r3, lr}
    /* If there's no new context to switch to, return early. */
    cbz r0, .Lreturn

    /* Write the suspending context to its stack pointer. */
    mrs r1, psp
    tst lr, 0x10
    it eq
    vstmdbeq r1!, {s16-s31}
    getpsplim
    stmdb r1!, {base-r11, lr}

    /* Store the new stack pointer with the saved context. */
    ldr r2, =rt_context_prev
    ldr r2, [r2]
    str r1, [r2]

    /* Load the new context returned by rt_syscall_run. */
    ldmia r0!, {base-r11, lr}
    setpsplim
    tst lr, 0x10
    it eq
    vldmiaeq r0!, {s16-s31}

    /* Set the new stack pointer. */
    msr psp, r0

.Lreturn:
    bx lr
