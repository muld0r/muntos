#ifndef RT_MPU_H
#define RT_MPU_H

#ifndef RT_MPU_ENABLE
#define RT_MPU_ENABLE 0
#endif

#if RT_MPU_ENABLE

#if __ARM_ARCH == 6 || __ARM_ARCH == 7

/*
 * In armv{6,7}, MPU regions have power-of-2 sizes with 8 subregions of equal
 * size. armv6 supports regions with size 256 or greater, and armv7 supports
 * regions with size 32 or greater, but only supports subregions for regions
 * with size 256 or greater. An MPU region of size 32 can be achieved with a
 * 256-byte region and one active subregion, so just use >= 256 bytes always.
 */
#define RT_MPU_SIZE(n)                                                         \
    (((n) <= 256) ? UINT32_C(7) : (UINT32_C(31) - __builtin_clzl(n)))

#define RT_MPU_REGION_SIZE(n) ((uint64_t)1 << (RT_MPU_SIZE(n) + 1))
#define RT_MPU_SUBREGION_SIZE(n) (RT_MPU_REGION_SIZE(n) / 8)

#define RT_MPU_SUBREGIONS(n) ((((n)-1) / RT_MPU_SUBREGION_SIZE(n)) + 1)

#define RT_MPU_SRD(n)                                                          \
    ((UINT32_C(0xFF00) >> (8 - RT_MPU_SUBREGIONS(n))) & UINT32_C(0xFF))

#define RT_MPU_ALIGN(n)                                                        \
    (RT_MPU_SUBREGIONS(n) > 4 ? RT_MPU_REGION_SIZE(n)                          \
     : RT_MPU_SUBREGIONS(n) > 2                                                \
         ? 4 * RT_MPU_SUBREGION_SIZE(n)                                        \
         : RT_MPU_SUBREGIONS(n) * RT_MPU_SUBREGION_SIZE(n))

#elif __ARM_ARCH == 8

/*
 * In armv8, MPU regions are defined by start and end addresses that are
 * multiples of 32 bytes.
 */
#define RT_MPU_ALIGN(n) 32UL

#else /* __ARM_ARCH */
#error "Unsupported __ARM_ARCH for MPU configuration."
#endif

#endif /* RT_MPU_ENABE */

#endif /* RT_MPU_H */
