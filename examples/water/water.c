#include "water.h"

#include <rt/atomic.h>
#include <rt/log.h>
#include <rt/rt.h>
#include <rt/sleep.h>
#include <rt/task.h>
#include <rt/tick.h>

static rt_atomic_uint32_t hydrogen_bonded = 0;
static rt_atomic_uint32_t oxygen_bonded = 0;
static rt_atomic_uint32_t water_formed = 0;

void make_water(void)
{
    rt_atomic_fetch_add_explicit(&water_formed, 1, memory_order_relaxed);
}

#define TICKS_TO_RUN 1000

static void timeout(void)
{
    rt_task_drop_privilege();
    rt_sleep(TICKS_TO_RUN);
    rt_stop();
}

static void oxygen_loop(void)
{
    rt_task_drop_privilege();
    for (;;)
    {
        oxygen();
        rt_atomic_fetch_add_explicit(&oxygen_bonded, 1, memory_order_relaxed);
    }
}

static void hydrogen_loop(void)
{
    rt_task_drop_privilege();
    for (;;)
    {
        hydrogen();
        rt_atomic_fetch_add_explicit(&hydrogen_bonded, 1, memory_order_relaxed);
    }
}

int main(void)
{
    RT_TASK(timeout, RT_STACK_MIN, 3);
    RT_TASK(hydrogen_loop, RT_STACK_MIN, 1);
    RT_TASK(hydrogen_loop, RT_STACK_MIN, 1);
    RT_TASK(oxygen_loop, RT_STACK_MIN, 2);

    rt_start();

    const uint32_t w = rt_atomic_load(&water_formed);
    const uint32_t h = rt_atomic_load(&hydrogen_bonded);
    const uint32_t o = rt_atomic_load(&oxygen_bonded);

    /* The oxygen or hydrogen may not have bonded by the time rt_stop is called
     * after making a water molecule, so allow for o and h to be one molecule's
     * worth below expected value or exactly equal to it. */
    const uint32_t o_lo = w - 1;
    const uint32_t o_hi = w;
    const uint32_t h_lo = (w - 1) * 2;
    const uint32_t h_hi = w * 2;

    rt_logf("hydrogen = %u, oxygen = %u, water = %u\n", (unsigned)h,
            (unsigned)o, (unsigned)w);

    if ((o < o_lo) || (o > o_hi) || (h < h_lo) || (h > h_hi))
    {
        return 1;
    }
}
