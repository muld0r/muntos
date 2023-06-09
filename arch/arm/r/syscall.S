@ vim:ft=arm

/*
 * The interrupt management code for the syscall handler depends on both the
 * type of interrupt controller and the source of the syscall interrupt, so we
 * provide a way to extend this code with assembly macros.
 * vic_syscall_start needs to mask and clear the syscall interrupt such that
 * another syscall cannot occur when IRQs are re-enabled. vic_syscall_finish
 * must unmask the syscall interrupt. Both are run with IRQs disabled.
 */

#include <muntos/mpu.h>

#include "vic.h"

#if RT_ARCH_ARM_R_VIC_TYPE == VIM_SSI
#include "vim_ssi.S"
#endif

#ifndef __thumb__
// arm mode doesn't have cbz, so make our own.
.macro cbz reg, label
    cmp \reg, 0
    beq \label
.endm
#endif

.macro mpuconfigure
#if RT_MPU_ENABLE
    ldr r1, =rt_mpu_config
    ldr r1, [r1]
    movs r2, RT_MPU_TASK_REGION_START_ID
    adds r5, r2, RT_MPU_NUM_TASK_REGIONS

.Lmpusetloop:
    // Set the region number register.
    mcr 15, 0, r2, cr6, cr2, 0

    // Load the region configuration and advance to the next one.
    ldmia r1!, {r3, r4}

    // Set the base address register.
    mcr 15, 0, r3, cr6, cr1, 0

    // Set the size and enable register.
    uxtb r3, r4
    mcr 15, 0, r3, cr6, cr1, 2

    // Set the access control register.
    lsrs r3, r4, 16
    mcr 15, 0, r3, cr6, cr1, 4

    add r2, 1
    cmp r2, r5
    blt .Lmpusetloop
    dsb
#endif
.endm

    .global rt_syscall_handler_irq
    .global rt_syscall_handler_svc
    .type rt_syscall_handler_irq, %function
    .type rt_syscall_handler_svc, %function
rt_syscall_handler_irq:
    sub lr, 4
rt_syscall_handler_svc:
    // Push lr (task pc) and spsr (task cpsr) to the system/user stack.
    srsdb 31!
    // Switch to system mode.
    cps 31
    // Save the task's volatile registers.
    push {r0-r3, r12, lr}
    vic_syscall_start

    // Switch to supervisor mode and re-enable interrupts.
    cpsie i, 19
    bl rt_syscall_run
    // Switch back to system mode to save and restore task contexts.
    cps 31

    // If there's no new context to switch to, return early.
    cbz r0, .Lreturn

    /* Ordinarily a clrex is necessary here, but rt_syscall_run uses strex,
     * which also clears the exclusive monitor. */

#ifdef __ARM_FP
    // Check if the task has a floating-point context.
    ldr r1, =rt_task_fp_enabled
    ldrb r2, [r1]

    // Store the task's floating-point context if used.
    cbz r2, .Lskip_fp_save
    vpush {d0-d15}
    vmrs r3, fpscr
    push {r3}
.Lskip_fp_save:

    // Store the rest of the task's context.
    push {r2, r4-r11} // flags, callee-saved registers
#else
    push {r4-r11}
#endif

    // Store the stack pointer with the saved context.
    ldr r2, =rt_context_prev
    ldr r2, [r2]
    str sp, [r2]

    mpuconfigure

    // Switch to the new task stack returned by rt_syscall_run.
    mov sp, r0

#ifdef __ARM_FP
    pop {r2, r4-r11}

    // Restore rt_task_fp_enabled.
    strb r2, [r1] // r1 still contains &rt_task_fp_enabled

    // Restore the task's floating-point context if used.
    cbz r2, .Lskip_fp_restore
    pop {r3}
    vmsr fpscr, r3
    vpop {d0-d15}
.Lskip_fp_restore:

#else
    pop {r4-r11}
#endif

.Lreturn:
    /* Disable interrupts again before unmasking the syscall interrupt at the
     * VIC; otherwise, repeated syscalls could grow the task stack
     * indefinitely. */
    cpsid i
    vic_syscall_finish

    // Restore the task's volatile registers.
    pop {r0-r3, r12, lr}
    // Restore the task's pc and cpsr (this re-enables interrupts).
    rfeia sp!
