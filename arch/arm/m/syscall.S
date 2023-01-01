@ vim:ft=arm

#include "exc_return.h"

    .syntax unified
    .thumb
    .p2align 1
    .section .text.rt_syscall_handler
    .global rt_syscall_handler
    .type rt_syscall_handler, %function
rt_syscall_handler:

#if (__ARM_ARCH == 6)

#include "syscall-v6.S"

#elif (__ARM_ARCH == 7)

#if defined(__ARM_FP)
#include "syscall-v7.S"
#else
#include "syscall-v7-nofp.S"
#endif

#elif (__ARM_ARCH == 8)

#if defined(__ARM_ARCH_8M_BASE__)
#include "syscall-v8-base.S"
#elif defined(__ARM_FP)
#include "syscall-v8.S"
#else
#include "syscall-v8-nofp.S"
#endif

#endif

    .size rt_syscall_handler, .-rt_syscall_handler
