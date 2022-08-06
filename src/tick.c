#include <rt/critical.h>
#include <rt/sleep.h>
#include <rt/tick.h>

#include <stdio.h>

static unsigned long rt_ticks;

void rt_tick_advance(void)
{
    //__atomic_add_fetch(&rt_ticks, 1, __ATOMIC_RELEASE);
    ++rt_ticks;
#ifdef RT_LOG
    printf("tick %lu\n", rt_ticks);
    fflush(stdout);
#endif
    rt_sleep_check();
    rt_yield();
}

unsigned long rt_tick(void)
{
    return rt_ticks;
    //return __atomic_load_n(&rt_ticks, __ATOMIC_ACQUIRE);
}
