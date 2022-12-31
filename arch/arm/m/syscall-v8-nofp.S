#include "arm-m.h"

    bl rt_syscall_run
    cbz r0, .Lreturn

    mrs r1, psp
    mrs r3, psplim
    stmdb r1!, {r3-r11}

    ldr r2, =rt_context_prev
    ldr r2, [r2]
    str r1, [r2]

    ldmia r0!, {r3-r11}
    msr psplim, r3
    msr psp, r0

.Lreturn:
    mov r0, TASK_INITIAL_EXC_RETURN
    bx r0
