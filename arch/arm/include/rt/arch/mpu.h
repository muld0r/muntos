#ifndef RT_ARCH_MPU_H
#define RT_ARCH_MPU_H

#include <rt/mpu.h>

#if RT_MPU_ENABLE && !defined(__ASSEMBLER__)

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifndef RT_MPU_NUM_REGIONS
#define RT_MPU_NUM_REGIONS 8
#endif

#ifndef RT_MPU_NUM_TASK_REGIONS
#define RT_MPU_NUM_TASK_REGIONS 4
#endif

// Per-task regions have higher region IDs so they can override static regions.
#define RT_MPU_TASK_REGION_START_ID                                            \
    (RT_MPU_NUM_REGIONS - RT_MPU_NUM_TASK_REGIONS)

#define RT_MPU_CTRL_ENABLE (UINT32_C(1) << 0)
#define RT_MPU_CTRL_HFNMI_ENABLE (UINT32_C(1) << 1)
#define RT_MPU_CTRL_PRIVDEF_ENABLE (UINT32_C(1) << 2)

#if __ARM_ARCH_PROFILE == 'R'
#include <../r/coprocessor.h>
#elif __ARM_ARCH_PROFILE == 'M'
#define RT_MPU_REGS ((volatile struct rt_mpu *)0xE000ED90UL)
#endif

#if __ARM_ARCH == 6 || __ARM_ARCH == 7

/*
 * In armv{6,7}, MPU regions have power-of-2 sizes with 8 subregions of equal
 * size. armv6 supports regions with size 256 or greater, and armv7 supports
 * regions with size 32 or greater, but only supports subregions for regions
 * with size 256 or greater. An MPU region of size 32 can be achieved with a
 * 256-byte region and one active subregion, so just use >= 256 bytes always.
 * Let 0 represent the maximum-size region (4GB).
 */
#define RT_MPU_SIZEBITS(n)                                                     \
    (((n) <= 256) ? UINT32_C(7)                                                \
     : ((n) == 0) ? UINT32_C(31)                                               \
                  : (UINT32_C(30) - (uint32_t)__builtin_clzl(n)))

#define RT_MPU_REGION_SIZE(n) (UINT32_C(1) << (RT_MPU_SIZEBITS(n) + 1))

#define RT_MPU_SUBREGION_SIZE(n) (RT_MPU_REGION_SIZE(n) / 8)

/* Only use subregions when the size is known at compile-time, to avoid
 * run-time division during configurations. Static task stacks should have this
 * property. Also require n > 0 because the calculations don't work for n == 0.
 */
#define RT_MPU_USE_SUBREGIONS(n) (__builtin_constant_p(n) && (n > 0))

#define RT_MPU_SUBREGIONS(n)                                                   \
    (RT_MPU_USE_SUBREGIONS(n) ? ((((n)-1) / RT_MPU_SUBREGION_SIZE(n)) + 1) : 8)

/* Calculate in which subregion an address resides, given the region size. */
#define RT_MPU_SUBREGION_OFFSET(a, n)                                          \
    ((((a) & ~(RT_MPU_SUBREGION_SIZE(n) - 1)) -                                \
      ((a) & ~(RT_MPU_REGION_SIZE(n) - 1))) /                                  \
     RT_MPU_SUBREGION_SIZE(n))

#define RT_MPU_SRD_PREFIX(o) ((UINT32_C(1) << (o)) - 1)
#define RT_MPU_SRD_SUFFIX(o)                                                   \
    ((~((UINT32_C(1) << ((o) + 1)) - 1)) & UINT32_C(0xFF))

#define RT_MPU_SRD(a, n)                                                       \
    (RT_MPU_USE_SUBREGIONS(n)                                                  \
         ? (RT_MPU_SRD_PREFIX(RT_MPU_SUBREGION_OFFSET(a, n)) |                 \
            RT_MPU_SRD_SUFFIX(RT_MPU_SUBREGION_OFFSET((a) + (n)-1, n)))        \
         : UINT32_C(0))

#define RT_MPU_ALIGN(n)                                                        \
    (((n) == 0)                 ? UINT64_C(0x100000000)                        \
     : RT_MPU_SUBREGIONS(n) > 4 ? RT_MPU_REGION_SIZE(n)                        \
     : RT_MPU_SUBREGIONS(n) > 2                                                \
         ? 4 * RT_MPU_SUBREGION_SIZE(n)                                        \
         : RT_MPU_SUBREGIONS(n) * RT_MPU_SUBREGION_SIZE(n))

#define RT_MPU_SIZE(n) (RT_MPU_SUBREGION_SIZE(n) * RT_MPU_SUBREGIONS(n))

struct rt_mpu_region
{
    uint32_t base_addr;
    uint32_t attr_size;
};

struct rt_mpu_config
{
    struct rt_mpu_region regions[RT_MPU_NUM_TASK_REGIONS];
};

#if __ARM_ARCH == 7
// v7-m implements alias registers, but v6-m does not.
#define RT_MPU_NUM_REGION_REGS 4
#else
#define RT_MPU_NUM_REGION_REGS 1
#endif

struct rt_mpu
{
    uint32_t type;
    uint32_t ctrl;
    uint32_t number;
    struct rt_mpu_region regions[RT_MPU_NUM_REGION_REGS];
};

#define RT_MPU_ATTR_XN (UINT32_C(1) << 28)

#define RT_MPU_ATTR_AP(p) ((uint32_t)(p) << 24)
#define RT_MPU_ATTR_NO_ACCESS RT_MPU_ATTR_AP(0)
#define RT_MPU_ATTR_RW_PRIV RT_MPU_ATTR_AP(1)
#define RT_MPU_ATTR_RW_PRIV_RO RT_MPU_ATTR_AP(2)
#define RT_MPU_ATTR_RW RT_MPU_ATTR_AP(3)
#define RT_MPU_ATTR_RO_PRIV RT_MPU_ATTR_AP(5)
#define RT_MPU_ATTR_RO RT_MPU_ATTR_AP(6)

#define RT_MPU_ATTR(texscb) ((uint32_t)(texscb) << 16)
#define RT_MPU_ATTR_STRONGLY_ORDERED RT_MPU_ATTR(0)
#define RT_MPU_ATTR_SHARED_DEVICE RT_MPU_ATTR(1)
#define RT_MPU_ATTR_CACHED_WT RT_MPU_ATTR(2)
#define RT_MPU_ATTR_CACHED_WB RT_MPU_ATTR(3)
#define RT_MPU_ATTR_NONCACHED RT_MPU_ATTR(8)
#define RT_MPU_ATTR_CACHED_WB_RWALLOC RT_MPU_ATTR(11)
#define RT_MPU_ATTR_DEVICE RT_MPU_ATTR(16)
#define RT_MPU_ATTR_CACHED(outer, inner)                                       \
    RT_MPU_ATTR((UINT32_C(1) << 5) | ((outer) << 3) | (inner))

// Only has an effect normal memory
#define RT_MPU_ATTR_SHARED (UINT32_C(1) << 18)

#define RT_MPU_ATTR_ENABLE (UINT32_C(1) << 0)

#define RT_MPU_STACK_ATTR                                                      \
    (RT_MPU_ATTR_RW | RT_MPU_ATTR_XN | RT_MPU_ATTR_CACHED_WB_RWALLOC |         \
     RT_MPU_ATTR_ENABLE)

#if __ARM_ARCH_PROFILE == 'R'
// v7-r doesn't use the lower 5 bits of the base address register
#define RT_MPU_VALID (UINT32_C(0))
#define RT_MPU_REGION_MASK (UINT32_C(0))
#elif __ARM_ARCH_PROFILE == 'M'
#define RT_MPU_VALID (UINT32_C(1) << 4)
#define RT_MPU_REGION_MASK (UINT32_C(0xF))
#endif

#define RT_MPU_BASE_ADDR(id, start_addr)                                       \
    (((id)&RT_MPU_REGION_MASK) | RT_MPU_VALID | (start_addr))

#define RT_MPU_ATTR_SIZE(start_addr, size, attr)                               \
    (RT_MPU_SIZEBITS(size) << 1 | RT_MPU_SRD(start_addr, size) << 8 | (attr))

static inline void rt_mpu_config_set(struct rt_mpu_config *config, uint32_t id,
                                     uintptr_t start_addr, size_t size,
                                     uint32_t attr)
{
    const uint32_t index = id - RT_MPU_TASK_REGION_START_ID;
    config->regions[index].base_addr = RT_MPU_BASE_ADDR(id, start_addr);
    config->regions[index].attr_size = RT_MPU_ATTR_SIZE(start_addr, size, attr);
}

static inline void rt_mpu_config_init(struct rt_mpu_config *config)
{
    for (uint32_t i = 0; i < RT_MPU_NUM_TASK_REGIONS; ++i)
    {
        /* Initialize the region number and valid bit for all task regions even
         * if the region will never be enabled, so that the configurations can
         * be applied safely. If the region registers are 0, the
         * previously-used MPU region number will be used when each
         * configuration is applied, disabling already-configured regions. */
        config->regions[i].base_addr =
            RT_MPU_BASE_ADDR(RT_MPU_TASK_REGION_START_ID + i, 0);
        config->regions[i].attr_size = RT_MPU_ATTR_SIZE(0, 256, 0);
    }
}

static inline void rt_mpu_region_set(uint32_t id, uintptr_t start_addr,
                                     size_t size, uint32_t attr)
{
#if __ARM_ARCH_PROFILE == 'R'
    rgnr_set(id);
    drbar_set(start_addr);
    const uint32_t attr_size = RT_MPU_ATTR_SIZE(start_addr, size, attr);
    // The size, subregion disable, and enable bit are in drsr.
    drsr_set(attr_size & UINT32_C(0xFFFF));
    dracr_set(attr_size >> 16);
#elif __ARM_ARCH_PROFILE == 'M'
    RT_MPU_REGS->regions[0].base_addr = RT_MPU_BASE_ADDR(id, start_addr);
    RT_MPU_REGS->regions[0].attr_size =
        RT_MPU_ATTR_SIZE(start_addr, size, attr);
#endif
}

#elif __ARM_ARCH == 8

/*
 * In armv8, MPU regions are defined by start and end addresses that are
 * multiples of 32 bytes.
 */
#define RT_MPU_ALIGN(n) 32UL

struct rt_mpu_region
{
    uint32_t base_addr;
    uint32_t limit_addr;
};

struct rt_mpu_config
{
    struct rt_mpu_region regions[RT_MPU_NUM_TASK_REGIONS];
};

#ifdef __ARM_ARCH_8M_BASE__
#define RT_MPU_NUM_REGION_REGS 1
#define RT_MPU_NUM_RESERVED 4
#else
#define RT_MPU_NUM_REGION_REGS 4
#define RT_MPU_NUM_RESERVED 1

#if RT_MPU_NUM_TASK_REGIONS > 4
#error "Only up to 4 task regions are supported on armv8-m.main"
/*
 * TODO: this can be implemented by updating the region number dynamically if
 * necessary.
 */
#endif

#endif

struct rt_mpu
{
    uint32_t type;
    uint32_t ctrl;
    uint32_t number;
    struct rt_mpu_region regions[RT_MPU_NUM_REGION_REGS];
    uint32_t reserved[RT_MPU_NUM_RESERVED];
    uint32_t attr_indirect[2];
};

#define RT_MPU_ATTR_XN (UINT32_C(1) << 0)

#define RT_MPU_ATTR_AP(p) ((uint32_t)(p) << 1)
#define RT_MPU_ATTR_RW_PRIV RT_MPU_ATTR_AP(0)
#define RT_MPU_ATTR_RW RT_MPU_ATTR_AP(1)
#define RT_MPU_ATTR_RO_PRIV RT_MPU_ATTR_AP(2)
#define RT_MPU_ATTR_RO RT_MPU_ATTR_AP(3)

#define RT_MPU_ATTR_SH(sh) ((uint32_t)(sh) << 3)
#define RT_MPU_ATTR_OUTER_SHAREABLE RT_MPU_ATTR_SH(2)
#define RT_MPU_ATTR_SHARED RT_MPU_ATTR_OUTER_SHAREABLE
#define RT_MPU_ATTR_INNER_SHAREABLE RT_MPU_ATTR_SH(3)

/* These attribute fields are in the limit_addr register, but to allow them to
 * be passed as part of a single attribute argument, shift them up by 5 bits. */
#define RT_MPU_ATTR_RLAR_SHIFT 5
#define RT_MPU_ATTR_MASK (UINT32_C(0x1F))
#define RT_MPU_ADDR_MASK (~RT_MPU_ATTR_MASK)
#define RT_MPU_ATTR_ENABLE (UINT32_C(1) << (0 + RT_MPU_ATTR_RLAR_SHIFT))
#define RT_MPU_ATTR_INDEX(index)                                               \
    ((uint32_t)(index) << (1 + RT_MPU_ATTR_RLAR_SHIFT))
#define RT_MPU_ATTR_PXN (UINT32_C(1) << (4 + RT_MPU_ATTR_RLAR_SHIFT))

#define RT_MPU_ATTR_INDIRECT(outer, inner)                                     \
    ((uint32_t)(outer) << 4 | (uint32_t)(inner))
#define RT_MPU_ATTR_DEVICE(gre) RT_MPU_ATTR_INDIRECT(0, (gre) << 2)
#define RT_MPU_ATTR_DEVICE_NGNRNE RT_MPU_ATTR_DEVICE(0)
#define RT_MPU_ATTR_DEVICE_NGNRE RT_MPU_ATTR_DEVICE(1)
#define RT_MPU_ATTR_DEVICE_NGRE RT_MPU_ATTR_DEVICE(2)
#define RT_MPU_ATTR_DEVICE_GRE RT_MPU_ATTR_DEVICE(3)

#define RT_MPU_ATTR_WT_TRANSIENT(r, w) ((uint32_t)(r) << 1 | (uint32_t)(w))

#define RT_MPU_ATTR_NONCACHEABLE RT_MPU_ATTR_INDIRECT(0x4, 0x4)

#define RT_MPU_ATTR_WB_TRANSIENT(r, w)                                         \
    (UINT32_C(0x4) | (uint32_t)(r) << 1 | (uint32_t)(w))

#define RT_MPU_ATTR_WB_RALLOC                                                  \
    RT_MPU_ATTR_INDIRECT(RT_MPU_ATTR_WB_TRANSIENT(1, 0),                       \
                         RT_MPU_ATTR_WB_TRANSIENT(1, 0))

#define RT_MPU_ATTR_WB_RWALLOC                                                 \
    RT_MPU_ATTR_INDIRECT(RT_MPU_ATTR_WB_TRANSIENT(1, 1),                       \
                         RT_MPU_ATTR_WB_TRANSIENT(1, 1))

#define RT_MPU_ATTR_WT(r, w)                                                   \
    (UINT32_C(0xC) | (uint32_t)(r) << 1 | (uint32_t)(w))

#define RT_MPU_ATTR_WT_RALLOC                                                  \
    RT_MPU_ATTR_INDIRECT(RT_MPU_ATTR_WT(1, 0), RT_MPU_ATTR_WT(1, 0))

#define RT_MPU_ATTR_WT_RWALLOC                                                 \
    RT_MPU_ATTR_INDIRECT(RT_MPU_ATTR_WT(1, 1), RT_MPU_ATTR_WT(1, 1))

// The stack will use the indirect attributes for index 0.
#define RT_MPU_STACK_ATTR (RT_MPU_ATTR_RW | RT_MPU_ATTR_XN | RT_MPU_ATTR_ENABLE)

#define RT_MPU_BASE_ADDR(start_addr, attr)                                     \
    ((start_addr & RT_MPU_ADDR_MASK) | (attr & RT_MPU_ATTR_MASK))

#define RT_MPU_LIMIT_ADDR(start_addr, size, attr)                              \
    (((start_addr + size - 1) & RT_MPU_ADDR_MASK) |                            \
     ((attr >> RT_MPU_ATTR_RLAR_SHIFT) & RT_MPU_ATTR_MASK))

static inline void rt_mpu_config_init(struct rt_mpu_config *config)
{
    memset(config, 0, sizeof *config);
}

static inline void rt_mpu_config_set(struct rt_mpu_config *config, uint32_t id,
                                     uintptr_t start_addr, size_t size,
                                     uint32_t attr)
{
    const uint32_t index = id - RT_MPU_TASK_REGION_START_ID;
    config->regions[index].base_addr = RT_MPU_BASE_ADDR(start_addr, attr);
    config->regions[index].limit_addr =
        RT_MPU_LIMIT_ADDR(start_addr, size, attr);
}

static inline void rt_mpu_region_set(uint32_t id, uintptr_t start_addr,
                                     size_t size, uint32_t attr)
{
    RT_MPU_REGS->number = id;
    RT_MPU_REGS->regions[0].base_addr = RT_MPU_BASE_ADDR(start_addr, attr);
    RT_MPU_REGS->regions[0].limit_addr =
        RT_MPU_LIMIT_ADDR(start_addr, size, attr);
}

static inline void rt_mpu_attr_init(void)
{
    RT_MPU_REGS->attr_indirect[0] = 0;
    RT_MPU_REGS->attr_indirect[1] = 0;
}

static inline void rt_mpu_attr_set(uint32_t index, uint32_t attr)
{
    volatile uint32_t *const mair = &RT_MPU_REGS->attr_indirect[index / 4];
    const int shift = (index % 4) * 8;
    const uint32_t mask = UINT32_C(0xFF) << shift;
    *mair = (*mair & ~mask) | (attr << shift);
}

#else /* __ARM_ARCH */
#error "Unsupported __ARM_ARCH for MPU configuration."
#endif

static inline void rt_mpu_enable(void)
{
#if __ARM_ARCH_PROFILE == 'R'
    sctlr_oreq(SCTLR_M | SCTLR_BR);
#elif __ARM_ARCH_PROFILE == 'M'
    RT_MPU_REGS->ctrl = RT_MPU_CTRL_ENABLE | RT_MPU_CTRL_HFNMI_ENABLE |
                        RT_MPU_CTRL_PRIVDEF_ENABLE;
#if __ARM_ARCH == 8 && !defined(__ARM_ARCH_8M_BASE__)
    /* Once the MPU is enabled, set the region number to the offset that will
     * be used for context switches because the region number is not part of
     * the region config on v8-m like it is in v7-m. On v8-m.base we need to
     * set the region number while reconfiguring anyway, so don't bother
     * setting an offset up front. */
    RT_MPU_REGS->number = RT_MPU_TASK_REGION_START_ID;
#endif // v8-m && !v8-m.base
#endif // __ARM_ARCH_PROFILE

    __asm__("dmb" ::: "memory");
    __asm__("isb");
}

static inline void rt_mpu_reconfigure(const struct rt_mpu_config *config)
{
#if __ARM_ARCH_PROFILE == 'R'
    for (uint32_t i = 0; i < RT_MPU_NUM_TASK_REGIONS; ++i)
    {
        rgnr_set(RT_MPU_TASK_REGION_START_ID + i);
        drbar_set(config->regions[i].base_addr);
        drsr_set(config->regions[i].attr_size & UINT32_C(0xFFFF));
        dracr_set(config->regions[i].attr_size >> 16);
    }
#elif __ARM_ARCH_PROFILE == 'M'

#if (RT_MPU_NUM_REGION_REGS == 4) && ((RT_MPU_NUM_TASK_REGIONS % 4) == 0)
    for (uint32_t i = 0; i < RT_MPU_NUM_TASK_REGIONS; i += 4)
    {
        __asm__("ldmia %0, {r4-r11}; stmia %1, {r4-r11}"
                :
                : "r"(&config->regions[i]), "r"(RT_MPU_REGS->regions)
                : "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11");
    }
#else // no alias registers, or # of task regions not a multiple of 4
    for (uint32_t i = 0; i < RT_MPU_NUM_TASK_REGIONS; ++i)
    {
#ifdef __ARM_ARCH_8M_BASE__
        /* armv8-m.base is unique in that it does not embed the region number in
         * the region config register, but also does not have region alias
         * registers, so we have to set the region number for each reconfigured
         * region here. */
        RT_MPU_REGS->number = RT_MPU_TASK_REGION_START_ID + i;
#endif
        RT_MPU_REGS->regions[i % RT_MPU_NUM_REGION_REGS] = config->regions[i];
    }
#endif

#endif // __ARM_ARCH_PROFILE
}

#endif // RT_MPU_ENABLE

#endif // RT_ARCH_MPU_H
