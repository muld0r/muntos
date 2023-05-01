#ifndef RT_MPU_H
#define RT_MPU_H

#ifndef RT_MPU_ENABLE
#define RT_MPU_ENABLE 0
#endif

#if RT_MPU_ENABLE

#include <rt/arch/mpu.h>

#else
/* Provide no-op versions for the static task initialization macros when
 * there's no MPU. */
#define rt_mpu_config_init(config)                                             \
    do                                                                         \
    {                                                                          \
    } while (0)

#define rt_mpu_config_set(config, id, start_addr, size, attr)                  \
    do                                                                         \
    {                                                                          \
    } while (0)

#endif

#endif /* RT_MPU_H */
