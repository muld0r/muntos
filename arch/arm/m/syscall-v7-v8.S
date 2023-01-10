@ vim:ft=arm

/*
 * This variant of the syscall handler supports armv7-m and armv8-m, with or
 * without floating-point.
 */

#if __ARM_ARCH == 8
/* In armv8-m, save and restore psplim using r3 as a temporary. */
#define base r3
#define getpsplim mrs r3, psplim
#define setpsplim msr psplim, r3
#else /* v7 */
#define base r4
#define getpsplim
#define setpsplim
#endif

#ifdef __ARM_FP
/*
 * For floating-point, save and restore lr, which contains the floating-point
 * context bit of the exception return value, and use that bit to decide if the
 * non-volatile fp context should be saved/loaded as well.
 */
#define pushlr push {r3, lr}
#define poplr pop {r3, lr}
#define saveregs stmdb r1!, {base-r11, lr}
#define loadregs ldmia r0!, {base-r11, lr}
#define savefp tst lr, 0x10; it eq; vstmdbeq r1!, {s16-s31}
#define loadfp tst lr, 0x10; it eq; vldmiaeq r0!, {s16-s31}
#define return bx lr
#else
/*
 * With nofp, lr can be discarded and we can reload the contant exception
 * return value and branch to it.
 */
#define pushlr
#define poplr
#define saveregs stmdb r1!, {base-r11}
#define loadregs ldmia r0!, {base-r11}
#define savefp
#define loadfp
#define return mov lr, TASK_INITIAL_EXC_RETURN; bx lr
#endif

    pushlr
    bl rt_syscall_run
    poplr
    /* If there's no new context to switch to, return early. */
    cbz r0, .Lreturn

    /* Write the suspending context to its stack pointer. */
    mrs r1, psp
    savefp
    getpsplim
    saveregs

    /* Store the new stack pointer with the saved context. */
    ldr r2, =rt_context_prev
    ldr r2, [r2]
    str r1, [r2]

    /* Load the new context returned by rt_syscall_run. */
    loadregs
    setpsplim
    loadfp

    /* Set the new stack pointer. */
    msr psp, r0

.Lreturn:
    return
