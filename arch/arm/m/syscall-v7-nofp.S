#include "arm-m.h"

    bl rt_syscall_run
    cbz r0, .Lreturn

    mrs r1, psp
    stmdb r1!, {r4-r11}

    ldr r2, =rt_prev_context
    ldr r2, [r2]
    str r1, [r2]

    ldmia r0!, {r4-r11}
    msr psp, r0

.Lreturn:
    mov r0, TASK_INITIAL_EXC_RETURN
    bx r0
