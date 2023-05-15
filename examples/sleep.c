#include <muntos/atomic.h>
#include <muntos/log.h>
#include <muntos/muntos.h>
#include <muntos/sem.h>
#include <muntos/sleep.h>
#include <muntos/task.h>
#include <muntos/tick.h>

static const int nloops = 5;
static rt_atomic_bool wrong_tick = false;

static void sleep_periodic(uintptr_t period)
{
    rt_task_drop_privilege();
    unsigned long last_wake_tick = 0;
    for (int i = 0; i < nloops; ++i)
    {
        rt_sleep_periodic(&last_wake_tick, period);
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
    RT_TASK_ARG(sleep_periodic, 5, RT_STACK_MIN, 2);
    RT_TASK_ARG(sleep_periodic, 10, RT_STACK_MIN, 1);
    rt_start();

    if (rt_atomic_load(&wrong_tick))
    {
        return 1;
    }
}
