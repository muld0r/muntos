#pragma once

#if !defined(__ARM_ARCH)
#error "__ARM_ARCH must be defined"
#endif

#if (__ARM_ARCH == 8)

#if defined(__ARM_ARCH_8M_BASE__)
#define RT_ARM_M_PSPLIM 0
#else
#define RT_ARM_M_PSPLIM 1
#endif

// non-secure stack, thread mode, no FP, use PSP, non-secure exception
#define TASK_INITIAL_EXC_RETURN -68

#elif (__ARM_ARCH == 7) || (__ARM_ARCH == 6)

#define RT_ARM_M_PSPLIM 0

// thread mode, no FP, use PSP
#define TASK_INITIAL_EXC_RETURN -3

#else
#error "unsupported __ARM_ARCH " __ARM_ARCH
#endif
