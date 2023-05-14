@ vim:ft=arm

/*
 * This variant of the syscall handler supports armv{7,8}-m{,+nofp}.
 */

#include <rt/mpu.h>

#include "exc_return.h"

#ifdef __ARM_FP
/*
 * For floating-point, save and restore lr, which contains the exception return
 * value, and use the floating-point context bit of this value to decide if the
 * non-volatile fp context should be saved/restored as well.
 */
#define excreturn ,lr
#define pushlr push {r3, lr} // push r3 as well to keep sp 8-byte aligned
#define poplr pop {r3, lr}
#define savefp tst lr, 0x10; it eq; vstmdbeq r1!, {s16-s31}
#define loadfp tst lr, 0x10; it eq; vldmiaeq r0!, {s16-s31}
#define return bx lr
#else
/*
 * With nofp, lr can be clobbered and reloaded from an immediate because the
 * exception return value for tasks is a constant.
 */
#define excreturn
#define pushlr
#define poplr
#define savefp
#define loadfp
#define return mov r0, TASK_INITIAL_EXC_RETURN; bx r0
#endif

#if RT_MPU_ENABLE
#define controltemp r2,
#define getcontrol mrs r2, control
#define setcontrol msr control, r2

.macro mpuconfigure
    ldr r1, =rt_mpu_config
    ldr r1, [r1]
    ldr r2, =0xE000ED9C
    mpuset RT_MPU_TASK_REGION_START_ID, RT_MPU_NUM_TASK_REGIONS
    dsb
.endm

.macro mpuset r, n
    .if \n == 0
    .exitm
    .endif

#if (__ARM_ARCH == 8) && (RT_MPU_NUM_TASK_REGIONS > 4)
    .if (\r == RT_MPU_TASK_REGION_START_ID) || ((\r % 4) == 0)
    mov r3, \r & 0xFC
    str r3, [r2, -4]
    .endif
#endif

    .if ((\r % 4) == 0) && (\n >= 4)
    ldmia r1!, {r4-r11}
    stmia r2, {r4-r11}
    mpuset (\r + 4), (\n - 4)
    .elseif ((\r % 4) == 1) && (\n >= 3)
    ldmia r1!, {r4-r9}
#if __ARM_ARCH == 8
    add r3, r2, 4
    stmia r3, {r4-r9}
#else
    stmia r2, {r4-r9}
#endif
    mpuset (\r + 3), (\n - 3)
    .elseif ((\r % 4) == 2) && (\n >= 2)
    ldmia r1!, {r4-r7}
#if __ARM_ARCH == 8
    add r3, r2, 8
    stmia r3, {r4-r7}
#else
    stmia r2, {r4-r7}
#endif
    mpuset (\r + 2), (\n - 2)
    .else
    ldmia r1!, {r4-r5}
#if __ARM_ARCH == 8
    strd r4, r5, [r2, (\r % 4) * 4]
#else
    strd r4, r5, [r2]
#endif
    mpuset (\r + 1), (\n - 1)
    .endif
.endm

#else
#define controltemp
#define getcontrol
#define setcontrol
#define mpuconfigure
#endif

#if __ARM_ARCH == 8
#define psplimtemp r3,
#define getpsplim mrs r3, psplim
#define setpsplim msr psplim, r3
#else // v7
#define psplimtemp
#define getpsplim
#define setpsplim
#endif

#define saveregs stmdb r1!, {controltemp psplimtemp r4-r11 excreturn}
#define loadregs ldmia r0!, {controltemp psplimtemp r4-r11 excreturn}

    pushlr
    bl rt_syscall_run
    poplr
    // If there's no new context to switch to, return early.
    cbz r0, .Lreturn

    // Write the suspending context to its stack pointer.
    mrs r1, psp
    savefp
    getcontrol
    getpsplim
    saveregs

    // Store the new stack pointer with the saved context.
    ldr r2, =rt_context_prev
    ldr r2, [r2]
    str r1, [r2]

    mpuconfigure

    // Load the new context returned by rt_syscall_run.
    loadregs
    loadfp

    // Set the new stack pointer.
    msr psp, r0
    setcontrol
    setpsplim

.Lreturn:
    return
