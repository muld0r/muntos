#include <rt/sleep.h>
#include <rt/tick.h>
#include <rt/rt.h>

#include <stdatomic.h>

static atomic_bool wrong_tick = false;

static void sleep(void *arg)
{
    int n = 5;
    unsigned long last_wake_tick = 0;
    while (n > 0)
    {
        rt_sleep_periodic(&last_wake_tick, 10);
        if (rt_tick() != last_wake_tick)
        {
            atomic_store(&wrong_tick, true);
        }
        --n;
    }

    /* Only the second task to finish will call rt_stop. */
    if (!rt_sem_trywait(arg))
    {
        rt_stop();
    }
}

int main(void)
{
    static RT_SEM(stop_sem, 1);
    __attribute__((aligned(STACK_ALIGN))) static char stack0[TASK_STACK_SIZE],
        stack1[TASK_STACK_SIZE];
    RT_TASK(sleep, &stop_sem, stack0, 1);
    RT_TASK(sleep, &stop_sem, stack1, 1);
    rt_start();

    if (atomic_load(&wrong_tick))
    {
        return 1;
    }
}
