@ vim:ft=arm

    .syntax unified
    .section .text.rt_syscall_handler,"ax",%progbits
    .global rt_syscall_handler
    .type rt_syscall_handler, %function
rt_syscall_handler:

#if __ARM_ARCH_PROFILE == 'R'

#include "r/syscall.S"

#elif __ARM_ARCH_PROFILE == 'M'

#if __ARM_ARCH == 6 || defined(__ARM_ARCH_8M_BASE__)
#include "m/syscall-v6-v8base.S"
#else
#include "m/syscall-v7-v8.S"
#endif

#endif // __ARM_ARCH_PROFILE

    .size rt_syscall_handler, .-rt_syscall_handler
