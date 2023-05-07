@ vim:ft=arm

/*
 * This variant of the syscall handler supports armv6-m and armv8-m.base.
 *
 * armv8-m.base also supports cbz while armv6-m does not, but otherwise they
 * support the same subset of armv{7,8}-m that makes the save/restore procedure
 * different, mainly due to the lack of stmdb and ldm.w/stm.w.
 */

#include <rt/mpu.h>

#if RT_MPU_ENABLE
#define CONTROL_SIZE 4
#define controltemp r2,
#define getcontrol mrs r2, control
#define setcontrol msr control, r2
#else
#define CONTROL_SIZE 0
#define controltemp
#define getcontrol
#define setcontrol
#endif

#if __ARM_ARCH == 8
#define PSPLIM_SIZE 4
#define psplimtemp r3,
#define getpsplim mrs r3, psplim
#define setpsplim msr psplim, r3
#define returnifnull cbz r0, .Lreturn
#else /* v6 */
#define PSPLIM_SIZE 0
#define psplimtemp
#define getpsplim
#define setpsplim
#define returnifnull cmp r0, 0; beq .Lreturn
#endif

#define CONTEXT_SIZE (32 + PSPLIM_SIZE + CONTROL_SIZE)

    bl rt_syscall_run
    // If there's no new context to switch to, return early.
    returnifnull

    // Store the suspending task's context to its stack.
    mrs r1, psp
    subs r1, CONTEXT_SIZE

    ldr r2, =rt_context_prev
    ldr r2, [r2]
    str r1, [r2]

    getcontrol
    getpsplim
    stmia r1!, {controltemp psplimtemp r4-r7}
    mov r4, r8
    mov r5, r9
    mov r6, r10
    mov r7, r11
    stmia r1!, {r4-r7}

    // Load the new context returned by rt_syscall_run.
    mov r1, r0
    adds r0, CONTEXT_SIZE - 16
    ldmia r0!, {r4-r7}
    mov r8, r4
    mov r9, r5
    mov r10, r6
    mov r11, r7
    ldmia r1!, {controltemp psplimtemp r4-r7}

    // Set the new stack pointer.
    msr psp, r0
    setcontrol
    setpsplim

.Lreturn:
    movs r0, -TASK_INITIAL_EXC_RETURN
    negs r0, r0
    bx r0
