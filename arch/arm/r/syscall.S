@ vim:ft=arm

/*
 * The interrupt management code for the syscall handler depends on both the
 * type of interrupt controller and the source of the syscall interrupt, so we
 * provide a way to extend this code with assembly macros.
 * vic_syscall_start needs to clear and mask the syscall interrupt such that
 * another syscall cannot occur when IRQs are re-enabled. vic_syscall_finish
 * must unmask the syscall interrupt. Both are run with IRQs disabled, and may
 * use r0-r2 as scratch registers.
 */

#include "vic.h"

#if RT_ARCH_ARM_R_VIC_TYPE == VIM_SSI
#include "vim_ssi.S"
#endif

    .syntax unified
    .section .text.rt_syscall_handler
    .global rt_syscall_handler
    .type rt_syscall_handler, %function
rt_syscall_handler:
    sub lr, 4

    // Push irq lr (task pc) and spsr (task cpsr) to the system stack.
    srsdb 31!
    // Switch to system mode.
    cps 31
    // Push only some registers to the stack first so we can re-enable IRQs.
    push {r0-r2}

    vic_syscall_start
    cpsie i

    /* Align the stack pointer to 8 bytes and save the adjustment in r0 along
     * with the rest of the task's volatile registers. */
#ifdef __thumb__
    // thumb doesn't support and with sp.
    mov r1, sp
    and r0, r1, 4
#else
    and r0, sp, 4
#endif
    sub sp, r0
    push {r0, r3, r12, lr}

    blx rt_syscall_run

    // If there's no new context to switch to, return early.
    cmp r0, 0
    beq .Lreturn

    /* Ordinarily a clrex is necessary here, but rt_syscall_run uses strex,
     * which also clears the exclusive monitor. */

#ifdef __ARM_FP
    // Get the task's flags.
    ldr r1, =rt_context_flags
    ldr r2, [r1]

    // Store the task's floating-point context if used.
    tst r2, 1
    ittt ne
    vpushne {d0-d15}
    vmrsne r3, fpscr
    pushne {r3}

    // Store the rest of the task's context.
    push {r2, r4-r11} // flags, callee-saved registers
#else
    push {r4-r11}
#endif

    // Store the stack pointer with the saved context.
    ldr r2, =rt_context_prev
    ldr r2, [r2]
    str sp, [r2]

    // Switch to the new task stack returned by rt_syscall_run.
    mov sp, r0

#ifdef __ARM_FP
    pop {r2, r4-r11}

    // Restore the task's flags.
    str r2, [r1] // r1 still contains &rt_context_flags

    // Restore the task's floating-point context if used.
    tst r2, 1
    ittt ne
    popne {r3}
    vmsrne fpscr, r3
    vpopne {d0-d15}
#else
    pop {r4-r11}
#endif

.Lreturn:
    /* Restore the second group of volatile registers and undo the stack
     * adjustment with interrupts still enabled. */
    pop {r0, r3, r12, lr}
    add sp, r0

    /* Disable interrupts again before unmasking the syscall interrupt at the
     * VIC; otherwise, repeated syscalls could grow the task stack
     * indefinitely. */
    cpsid i
    vic_syscall_finish

    // Restore the remaining volatile registers.
    pop {r0-r2}
    // Restore the task's pc and cpsr (this re-enables interrupts).
    rfeia sp!

    .size rt_syscall_handler, .-rt_syscall_handler
