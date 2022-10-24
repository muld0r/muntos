#include <stdbool.h>

unsigned __atomic_exchange_4(volatile void *ptr, unsigned val, int memorder)
{
    (void)memorder;

    volatile unsigned *const p = ptr;

    __asm__("cpsid i\n"
            "dmb");
    unsigned ret = *p;
    *p = val;
    __asm__("dmb\n"
            "cpsie i");

    return ret;
}

unsigned __atomic_fetch_add_4(volatile void *ptr, unsigned val, int memorder)
{
    (void)memorder;

    volatile unsigned *const p = ptr;

    __asm__("cpsid i\n"
            "dmb");
    unsigned old = *p;
    *p = old + val;
    __asm__("dmb\n"
            "cpsie i");

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

    __asm__("cpsid i\n"
            "dmb");

    unsigned old = *p;
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
    __asm__("dmb\n"
            "cpsie i");

    return ret;
}
