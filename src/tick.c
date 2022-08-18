#include <rt/critical.h>
#include <rt/sleep.h>
#include <rt/tick.h>

#include <stdatomic.h>

static atomic_ulong rt_ticks;

void rt_tick_advance(void)
{
    atomic_fetch_add_explicit(&rt_ticks, 1, memory_order_release);
    rt_sleep_check();
    rt_yield();
}

unsigned long rt_tick(void)
{
    return atomic_load_explicit(&rt_ticks, memory_order_consume);
}
