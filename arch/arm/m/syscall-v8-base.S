    bl rt_syscall_run
    /* If there's no new context to switch to, return early. */
    cmp r0, 0
    beq .Lreturn

    /* Write the rest of the suspending task's register context to its stack
     * pointer, starting with psplim, r4-r7 at the bottom. */
    mrs r1, psp
    mrs r3, psplim
    subs r1, 36
    stm r1!, {r3-r7}

    /* Use the address of the saved r8-r11 registers as the context pointer so
     * they can be popped first on resume. */
    ldr r2, =rt_prev_context
    ldr r2, [r2]
    str r1, [r2]

    mov r4, r8
    mov r5, r9
    mov r6, r10
    mov r7, r11
    stm r1!, {r4-r7}

    /* Load the new register context returned by rt_syscall_run, which
     * initially points to the saved r8-r11. */
    ldm r0!, {r4-r7}
    mov r8, r4
    mov r9, r5
    mov r10, r6
    mov r11, r7

    /* Set the new stack pointer. */
    msr psp, r0

    /* Go back and load psplim, r4-r7. */
    subs r0, 36
    ldm r0!, {r3-r7}
    msr psplim, r3

.Lreturn:
    /* The thread exception return value is always -3 == 0xFFFFFFFD. */
    movs r0, 3
    negs r0, r0
    bx r0
