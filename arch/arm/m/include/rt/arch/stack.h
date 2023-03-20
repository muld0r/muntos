#ifndef RT_ARCH_STACK_H
#define RT_ARCH_STACK_H

#include <rt/arch/mpu.h>

#if RT_MPU_ENABLE

#define RT_STACK_ALIGN(n) RT_MPU_ALIGN(n)

#if __ARM_ARCH == 6 || __ARM_ARCH == 7
#define RT_STACK_SIZE(n) (RT_MPU_SUBREGION_SIZE(n) * RT_MPU_SUBREGIONS(n))
#endif

#endif /* RT_MPU_ENABLE */

/* The Arm ABI specifies a minimum stack alignment of 8 bytes. */
#ifndef RT_STACK_ALIGN
#define RT_STACK_ALIGN(n) 8UL
#endif

#define RT_STACK_MIN 160

#endif /* RT_ARCH_STACK_H */
