#include <rt/critical.h>
#include <rt/sleep.h>
#include <rt/tick.h>

#include <stdio.h>

static unsigned long rt_ticks;

void rt_tick_advance(void)
{
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
    rt_critical_begin();
    unsigned long count = rt_ticks;
    rt_critical_end();
    return count;
}
