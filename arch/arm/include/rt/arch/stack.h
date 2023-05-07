#pragma once

#include <rt/mpu.h>

#if RT_MPU_ENABLE

#define RT_STACK_ALIGN(n) RT_MPU_ALIGN(n)

#if __ARM_ARCH == 6 || __ARM_ARCH == 7
#define RT_STACK_SIZE(n) RT_MPU_SIZE(n)
#endif

#endif // RT_MPU_ENABLE

// The Arm ABI specifies a minimum stack alignment of 8 bytes.
#ifndef RT_STACK_ALIGN
#define RT_STACK_ALIGN(n) 8UL
#endif

/*
 * A register context takes 16-18 words depending on the architecture
 * subvariant. Pushing all callee-saved registers once requires an
 * additional 8 words. A task function that uses no stack space of its
 * own can use just the context size of 64-72 bytes.
 */
#define RT_STACK_MIN 128
