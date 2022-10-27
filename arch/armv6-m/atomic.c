#include <stdbool.h>

static void atomic_start(int memorder)
{
    __asm__("cpsid i");
    if ((memorder == __ATOMIC_RELEASE) || (memorder == __ATOMIC_ACQ_REL) ||
        (memorder == __ATOMIC_SEQ_CST))
    {
        __asm__("dmb sy");
    }
}

static void atomic_end(int memorder)
{
    if ((memorder == __ATOMIC_CONSUME) || (memorder == __ATOMIC_ACQUIRE) ||
        (memorder == __ATOMIC_ACQ_REL) || (memorder == __ATOMIC_SEQ_CST))
    {
        __asm__("dmb sy");
    }
    __asm__("cpsie i");
}

unsigned char __atomic_exchange_1(volatile void *ptr, unsigned char val,
                                  int memorder)
{
    volatile unsigned char *const p = ptr;

    atomic_start(memorder);
    const unsigned char old = *p;
    *p = val;
    atomic_end(memorder);

    return old;
}

unsigned __atomic_exchange_4(volatile void *ptr, unsigned val, int memorder)
{
    volatile unsigned *const p = ptr;

    atomic_start(memorder);
    const unsigned old = *p;
    *p = val;
    atomic_end(memorder);

    return old;
}

unsigned __atomic_fetch_add_4(volatile void *ptr, unsigned val, int memorder)
{
    volatile unsigned *const p = ptr;

    atomic_start(memorder);
    const unsigned old = *p;
    *p = old + val;
    atomic_end(memorder);

    return old;
}

bool __atomic_compare_exchange_4(volatile void *ptr, void *exp, unsigned val,
                                 bool weak, int success_memorder,
                                 int fail_memorder)
{
    (void)weak;

    volatile unsigned *const p = ptr;
    unsigned *const e = exp;

    atomic_start(success_memorder);
    const unsigned old = *p;
    if (old == *e)
    {
        *p = val;
        atomic_end(success_memorder);
        return true;
    }
    *e = old;
    atomic_end(fail_memorder);
    return false;
}
