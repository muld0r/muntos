#include "water.h"

#include <rt/atomic.h>
#include <rt/rt.h>
#include <rt/sleep.h>
#include <rt/task.h>
#include <rt/tick.h>

#include <stdbool.h>

static rt_atomic_uint hydrogen_bonded = 0;
static rt_atomic_uint oxygen_bonded = 0;
static rt_atomic_uint water_formed = 0;

void make_water(void)
{
    rt_atomic_fetch_add_explicit(&water_formed, 1, memory_order_relaxed);
}

#define TICKS_TO_RUN 1000

static void timeout(uintptr_t arg)
{
    (void)arg;
    rt_sleep(TICKS_TO_RUN + 5);
    rt_stop();
}

static void oxygen_loop(uintptr_t arg)
{
    (void)arg;
    while (rt_tick() < TICKS_TO_RUN)
    {
        oxygen();
        rt_atomic_fetch_add_explicit(&oxygen_bonded, 1, memory_order_relaxed);
    }
}

static void hydrogen_loop(uintptr_t arg)
{
    (void)arg;
    while (rt_tick() < TICKS_TO_RUN)
    {
        hydrogen();
        rt_atomic_fetch_add_explicit(&hydrogen_bonded, 1, memory_order_relaxed);
    }
}

int main(void)
{
    static char timeout_stack[TASK_STACK_SIZE]
        __attribute__((aligned(STACK_ALIGN)));
    RT_TASK(timeout, timeout_stack, 0);

    static char atom_stacks[3][TASK_STACK_SIZE]
        __attribute__((aligned(STACK_ALIGN)));
    RT_TASK(hydrogen_loop, atom_stacks[0], 1);
    RT_TASK(hydrogen_loop, atom_stacks[1], 1);
    RT_TASK(oxygen_loop, atom_stacks[2], 1);

    rt_start();

    unsigned w = rt_atomic_load(&water_formed);
    unsigned h = rt_atomic_load(&hydrogen_bonded);
    unsigned o = rt_atomic_load(&oxygen_bonded);

    if ((w != o) || (w * 2 != h))
    {
        return 1;
    }
}
