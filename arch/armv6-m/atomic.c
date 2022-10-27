#include <stdbool.h>

#define atomic_start()                                                         \
    __asm__("cpsid i\n"                                                        \
            "dmb")

#define atomic_end()                                                           \
    __asm__("dmb\n"                                                            \
            "cpsie i")

unsigned __atomic_exchange_4(volatile void *ptr, unsigned val, int memorder)
{
    (void)memorder;

    volatile unsigned *const p = ptr;

    atomic_start();
    unsigned ret = *p;
    *p = val;
    atomic_end();

    return ret;
}

unsigned __atomic_fetch_add_4(volatile void *ptr, unsigned val, int memorder)
{
    (void)memorder;

    volatile unsigned *const p = ptr;

    atomic_start();
    const unsigned old = *p;
    *p = old + val;
    atomic_end();

    return old;
}

bool __atomic_compare_exchange_4(volatile void *ptr, void *exp, unsigned val,
                                 bool weak, int success_memorder,
                                 int fail_memorder)
{
    (void)weak;
    (void)success_memorder;
    (void)fail_memorder;

    volatile unsigned *const p = ptr;
    unsigned *const e = exp;
    bool ret;

    atomic_start();
    const unsigned old = *p;
    if (old == *e)
    {
        *p = val;
        ret = true;
    }
    else
    {
        *e = old;
        ret = false;
    }
    atomic_end();

    return ret;
}
