#include <rt/critical.h>
#include <rt/delay.h>
#include <rt/tick.h>

static rt_tick_t rt_ticks;

void rt_tick(void)
{
    rt_critical_begin();
    ++rt_ticks;
    rt_delay_wake_tasks();
    rt_yield();
    rt_critical_end();
}

rt_tick_t rt_tick_count(void)
{
    rt_critical_begin();
    rt_tick_t count = rt_ticks;
    rt_critical_end();
    return count;
}
