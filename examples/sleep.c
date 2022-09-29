#include <rt/sleep.h>
#include <rt/tick.h>
#include <rt/rt.h>

#include <stdatomic.h>

static const int nloops = 5;
static atomic_bool wrong_tick = false;

static void sleep(void *arg)
{
    unsigned long last_wake_tick = 0;
    unsigned long *period = arg;
    for (int i = 0; i < nloops; ++i)
    {
        rt_sleep_periodic(&last_wake_tick, *period);
        if (rt_tick() != last_wake_tick)
        {
            atomic_store(&wrong_tick, true);
        }
    }

    /* Only the second task to finish will call rt_stop. */
    static RT_SEM(stop_sem, 1);
    if (!rt_sem_trywait(&stop_sem))
    {
        rt_stop();
    }
}

int main(void)
{
    __attribute__((aligned(STACK_ALIGN))) static char stack0[TASK_STACK_SIZE],
        stack1[TASK_STACK_SIZE];
    static unsigned long period0 = 5, period1 = 10;
    RT_TASK(sleep, &period0, stack0, 1);
    RT_TASK(sleep, &period1, stack1, 1);
    rt_start();

    if (atomic_load(&wrong_tick))
    {
        return 1;
    }
}
