@ vim:ft=arm

/*
 * This file provides the entry and exit sequences for the syscall handler
 * specific to the Vectored Interrupt Manager (VIM) and the System Software
 * Interrupt (SSI), as implemented on the TI Hercules family (TMS570/RM4).
 */

/*
 * The SSI should be routed to channel 126, which is the lowest priority.
 * This maps to bit 30 of the REQENA*3 registers.
 * Note: this is not the SSI's default VIM channel.
 */
#ifndef RT_ARCH_ARM_R_VIM_SSI_CHANNEL
#define SSI_CHANNEL 126
#else
#define SSI_CHANNEL RT_ARCH_ARM_R_VIM_SSI_CHANNEL
#endif

#define VIM_REQENASET0 0xFFFFFE30
#define VIM_REQENACLR0 0xFFFFFE40

#define SSI_CHANNEL_BITMASK (1 << (SSI_CHANNEL % 32))
#define SSI_CHANNEL_REG_OFFSET ((SSI_CHANNEL / 32) * 4)
#define SSI_REQENASET (VIM_REQENASET0 + SSI_CHANNEL_REG_OFFSET)
#define SSI_REQENACLR (VIM_REQENACLR0 + SSI_CHANNEL_REG_OFFSET)
#define SSIVEC 0xFFFFFFF4

/*
 * Use the address of the system control registers as a base address because it
 * can be loaded in a single instruction in both arm and thumb, and it allows
 * (+/-) 8-bit immediate offsets to access both the VIM and SSI registers.
 */
#define SYS_BASE 0xFFFFFF00

.macro vic_syscall_start
    mov r0, SYS_BASE
    // Mask the software interrupt in the VIM.
    mov r1, SSI_CHANNEL_BITMASK
    str r1, [r0, SSI_REQENACLR - SYS_BASE]
    // Clear the software interrupt.
    ldr r1, [r0, SSIVEC - SYS_BASE]
.endm

.macro vic_syscall_finish
    // Unmask the software interrupt in the VIM.
    mov r0, SYS_BASE
    mov r1, SSI_CHANNEL_BITMASK
    str r1, [r0, SSI_REQENASET - SYS_BASE]
.endm
