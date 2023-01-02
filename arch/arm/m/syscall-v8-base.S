@ vim:ft=arm

    bl rt_syscall_run
    /* If there's no new context to switch to, return early. */
    cmp r0, 0
    beq .Lreturn

    /* Store the suspending task's context to its stack. */
    mrs r1, psp
    mrs r3, psplim
    subs r1, 36

    ldr r2, =rt_context_prev
    ldr r2, [r2]
    str r1, [r2]

    stmia r1!, {r3-r7}
    mov r4, r8
    mov r5, r9
    mov r6, r10
    mov r7, r11
    stmia r1!, {r4-r7}

    /* Load the new context returned by rt_syscall_run. */
    mov r1, r0
    adds r1, 20
    ldmia r1!, {r4-r7}
    mov r8, r4
    mov r9, r5
    mov r10, r6
    mov r11, r7
    ldmia r0!, {r3-r7}
    msr psplim, r3
    msr psp, r1

.Lreturn:
    movs r0, -TASK_INITIAL_EXC_RETURN
    negs r0, r0
    bx r0
