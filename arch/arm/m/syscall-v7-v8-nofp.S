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

    bl rt_syscall_run
    cbz r0, .Lreturn

    mrs r1, psp
    getpsplim
    stmdb r1!, {base-r11}

    ldr r2, =rt_context_prev
    ldr r2, [r2]
    str r1, [r2]

    ldmia r0!, {base-r11}
    msr psp, r0
    setpsplim

.Lreturn:
    mov lr, TASK_INITIAL_EXC_RETURN
    bx lr
