#pragma once

#include <stdint.h>

#define COPROCESSOR_REG(name, coproc, opc1, crn, crm, opc2)                    \
    static inline uint32_t name(void)                                          \
    {                                                                          \
        uint32_t x;                                                            \
        __asm__ __volatile__("mrc " coproc ", %1, %0, " crn ", " crm ", %2"    \
                             : "=r"(x)                                         \
                             : "i"(opc1), "i"(opc2));                          \
        return x;                                                              \
    }                                                                          \
                                                                               \
    static inline void name##_set(uint32_t x)                                  \
    {                                                                          \
        __asm__("mcr " coproc ", %1, %0, " crn ", " crm ", %2"                 \
                :                                                              \
                : "r"(x), "i"(opc1), "i"(opc2));                               \
    }                                                                          \
                                                                               \
    static inline void name##_oreq(uint32_t x)                                 \
    {                                                                          \
        name##_set(name() | x);                                                \
    }                                                                          \
                                                                               \
    static inline void name##_andeq(uint32_t x)                                \
    {                                                                          \
        name##_set(name() & x);                                                \
    }

COPROCESSOR_REG(sctlr, "p15", 0, "c1", "c0", 0)
#define SCTLR_VE (UINT32_C(1) << 24)
#define SCTLR_TE (UINT32_C(1) << 30)

COPROCESSOR_REG(cpacr, "p15", 0, "c1", "c0", 2)
#define CPACR_10_FULL_ACCESS (UINT32_C(3) << 20)
#define CPACR_11_FULL_ACCESS (UINT32_C(3) << 22)

COPROCESSOR_REG(pmcr, "p15", 0, "c9", "c12", 0)
#define PMCR_E (UINT32_C(1) << 0) /* Enable. */
#define PMCR_C (UINT32_C(1) << 2) /* Cycle counter reset. */
#define PMCR_X (UINT32_C(1) << 4) /* Event export. */

COPROCESSOR_REG(actlr, "p15", 0, "c1", "c0", 1)
#define ACTLR_ATCMPCEN (UINT32_C(1) << 25)
#define ACTLR_SMOV (UINT32_C(1) << 7) /* Out-of-order divide disable. */
#define ACTLR_CEC_MASK (UINT32_C(7) << 3)
#define ACTLR_CEC_RECOVER (UINT32_C(5) << 3)
#define ACTLR_B0TCMPCEN (UINT32_C(1) << 26)
#define ACTLR_B1TCMPCEN (UINT32_C(1) << 27)

COPROCESSOR_REG(sactlr, "p15", 0, "c15", "c0", 0)
#define SACTLR_DOOFMACS (UINT32_C(1) << 16)

COPROCESSOR_REG(pmccntr, "p15", 0, "c9", "c13", 0)

COPROCESSOR_REG(pmcntenset, "p15", 0, "c9", "c12", 1)
COPROCESSOR_REG(pmcntenclr, "p15", 0, "c9", "c12", 2)
#define PMCNTEN_C (UINT32_C(1) << 31)
