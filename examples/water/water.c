#include "water.h"

#include <rt/atomic.h>
#include <rt/rt.h>
#include <rt/sleep.h>
#include <rt/task.h>
#include <rt/tick.h>

static rt_atomic_uint hydrogen_bonded = 0;
static rt_atomic_uint oxygen_bonded = 0;
static rt_atomic_uint water_formed = 0;

void make_water(void)
{
    rt_atomic_fetch_add_explicit(&water_formed, 1, memory_order_relaxed);
}

#define TICKS_TO_RUN 1000

static void timeout(void)
{
    rt_sleep(TICKS_TO_RUN);
    rt_stop();
}

static void oxygen_loop(void)
{
    for (;;)
    {
        oxygen();
        rt_atomic_fetch_add_explicit(&oxygen_bonded, 1, memory_order_relaxed);
    }
}

static void hydrogen_loop(void)
{
    for (;;)
    {
        hydrogen();
        rt_atomic_fetch_add_explicit(&hydrogen_bonded, 1, memory_order_relaxed);
    }
}

int main(void)
{
    static char timeout_stack[TASK_STACK_SIZE]
        __attribute__((aligned(STACK_ALIGN)));
    RT_TASK(timeout, timeout_stack, 4);

    static char atom_stacks[3][TASK_STACK_SIZE]
        __attribute__((aligned(STACK_ALIGN)));
    RT_TASK(hydrogen_loop, atom_stacks[0], 1);
    RT_TASK(hydrogen_loop, atom_stacks[1], 2);
    RT_TASK(oxygen_loop, atom_stacks[2], 3);

    rt_start();

    const unsigned w = rt_atomic_load(&water_formed);
    const unsigned h = rt_atomic_load(&hydrogen_bonded);
    const unsigned o = rt_atomic_load(&oxygen_bonded);

    /* The oxygen or hydrogen may not have bonded by the time rt_stop is called
     * after making a water molecule, so allow for o and h to be one molecule's
     * worth below expected value or exactly equal to it. */
    const unsigned o_lo = w - 1;
    const unsigned o_hi = w;
    const unsigned h_lo = (w - 1) * 2;
    const unsigned h_hi = w * 2;

    if ((o < o_lo) || (o > o_hi) || (h < h_lo) || (h > h_hi))
    {
        return 1;
    }
}
