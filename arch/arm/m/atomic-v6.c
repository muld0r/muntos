#include <stdbool.h>

// Disable interrupts and return the previous mask.
static inline bool disable(void)
{
    bool primask;
    __asm__ __volatile__("mrs %0, primask" : "=r"(primask));
    __asm__("cpsid i" ::: "memory");
    return primask;
}

static inline void restore(bool mask)
{
    __asm__("msr primask, %0" : : "r"(mask) : "memory");
}

static inline void barrier_start(int memorder)
{
    if ((memorder == __ATOMIC_RELEASE) || (memorder == __ATOMIC_ACQ_REL) ||
        (memorder == __ATOMIC_SEQ_CST))
    {
        __asm__("dmb sy" ::: "memory");
    }
}

static inline void barrier_end(int memorder)
{
    if ((memorder == __ATOMIC_CONSUME) || (memorder == __ATOMIC_ACQUIRE) ||
        (memorder == __ATOMIC_ACQ_REL) || (memorder == __ATOMIC_SEQ_CST))
    {
        __asm__("dmb sy" ::: "memory");
    }
}

unsigned char __atomic_exchange_1(volatile void *ptr, unsigned char val,
                                  int memorder)
{
    volatile unsigned char *const p = ptr;

    const bool mask = disable();
    barrier_start(memorder);
    const unsigned char old = *p;
    *p = val;
    barrier_end(memorder);
    restore(mask);

    return old;
}

unsigned __atomic_exchange_4(volatile void *ptr, unsigned val, int memorder)
{
    volatile unsigned *const p = ptr;

    const bool mask = disable();
    barrier_start(memorder);
    const unsigned old = *p;
    *p = val;
    barrier_end(memorder);
    restore(mask);

    return old;
}

unsigned __atomic_fetch_add_4(volatile void *ptr, unsigned val, int memorder)
{
    volatile unsigned *const p = ptr;

    const bool mask = disable();
    barrier_start(memorder);
    const unsigned old = *p;
    *p = old + val;
    barrier_end(memorder);
    restore(mask);

    return old;
}

unsigned __atomic_fetch_sub_4(volatile void *ptr, unsigned val, int memorder)
{
    volatile unsigned *const p = ptr;

    const bool mask = disable();
    barrier_start(memorder);
    const unsigned old = *p;
    *p = old - val;
    barrier_end(memorder);
    restore(mask);

    return old;
}

bool __atomic_compare_exchange_1(volatile void *ptr, void *exp,
                                 unsigned char val, bool weak,
                                 int success_memorder, int fail_memorder)
{
    (void)weak;

    volatile unsigned char *const p = ptr;
    unsigned char *const e = exp;

    const bool mask = disable();
    barrier_start(success_memorder);
    const unsigned char old = *p;
    const bool equal = old == *e;
    if (equal)
    {
        *p = val;
        barrier_end(success_memorder);
    }
    else
    {
        *e = old;
        barrier_end(fail_memorder);
    }
    restore(mask);
    return equal;
}

bool __atomic_compare_exchange_4(volatile void *ptr, void *exp, unsigned val,
                                 bool weak, int success_memorder,
                                 int fail_memorder)
{
    (void)weak;

    volatile unsigned *const p = ptr;
    unsigned *const e = exp;

    const bool mask = disable();
    barrier_start(success_memorder);
    const unsigned old = *p;
    const bool equal = old == *e;
    if (equal)
    {
        *p = val;
        barrier_end(success_memorder);
    }
    else
    {
        *e = old;
        barrier_end(fail_memorder);
    }
    restore(mask);
    return equal;
}
