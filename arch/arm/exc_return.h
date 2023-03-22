#pragma once

#ifndef __ARM_ARCH
#error "__ARM_ARCH must be defined"
#endif

#if __ARM_ARCH == 8

// non-secure stack, thread mode, no FP, use PSP, non-secure exception
#define TASK_INITIAL_EXC_RETURN -68

#elif (__ARM_ARCH == 7) || (__ARM_ARCH == 6)

// thread mode, no FP, use PSP
#define TASK_INITIAL_EXC_RETURN -3

#else
#error "unsupported __ARM_ARCH"
#endif
