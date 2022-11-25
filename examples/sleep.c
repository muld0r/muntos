#include <rt/atomic.h>
#include <rt/log.h>
#include <rt/rt.h>
#include <rt/sem.h>
#include <rt/sleep.h>
#include <rt/task.h>
#include <rt/tick.h>

static const int nloops = 5;
static rt_atomic_bool wrong_tick = false;

static void sleep(void *arg)
{
    unsigned long last_wake_tick = 0;
    unsigned long *period = arg;
    for (int i = 0; i < nloops; ++i)
    {
        rt_sleep_periodic(&last_wake_tick, *period);
        unsigned long wake_tick = rt_tick();
        if (wake_tick != last_wake_tick)
        {
            rt_logf("expected to wake at %lu, instead woke at %lu\n",
                    last_wake_tick, wake_tick);
            rt_atomic_store(&wrong_tick, true);
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
    static char task_stacks[2][TASK_STACK_SIZE]
        __attribute__((aligned(STACK_ALIGN)));
    static unsigned long period0 = 5, period1 = 10;
    RT_TASK(sleep, &period0, task_stacks[0], 0);
    RT_TASK(sleep, &period1, task_stacks[1], 1);
    rt_start();

    if (rt_atomic_load(&wrong_tick))
    {
        return 1;
    }
}
