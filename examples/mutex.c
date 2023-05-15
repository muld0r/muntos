#include <muntos/mutex.h>
#include <muntos/muntos.h>
#include <muntos/sem.h>
#include <muntos/sleep.h>
#include <muntos/task.h>

static RT_MUTEX(mutex);
static unsigned long x = 0;

#define NUM_TASKS 3
#define ITERATIONS 100000UL

static void stop_last(void)
{
    static RT_SEM(stop_sem, NUM_TASKS - 1);
    /* Only the last task to finish will call rt_stop. */
    if (!rt_sem_trywait(&stop_sem))
    {
        rt_stop();
    }
}

static void increment_lock(void)
{
    rt_task_drop_privilege();
    for (unsigned long i = 0; i < ITERATIONS; ++i)
    {
        rt_mutex_lock(&mutex);
        ++x;
        rt_mutex_unlock(&mutex);
    }
    stop_last();
}

static void increment_trylock(void)
{
    rt_task_drop_privilege();
    for (unsigned long i = 0; i < ITERATIONS; ++i)
    {
        while (!rt_mutex_trylock(&mutex))
        {
            rt_sleep(1);
        }
        ++x;
        rt_mutex_unlock(&mutex);
    }
    stop_last();
}

static void increment_timedlock(void)
{
    rt_task_drop_privilege();
    for (unsigned long i = 0; i < ITERATIONS; ++i)
    {
        while (!rt_mutex_timedlock(&mutex, 1))
        {
        }
        ++x;
        rt_mutex_unlock(&mutex);
    }
    stop_last();
}

int main(void)
{
    RT_TASK(increment_lock, RT_STACK_MIN, 1);
    RT_TASK(increment_trylock, RT_STACK_MIN, 1);
    RT_TASK(increment_timedlock, RT_STACK_MIN, 1);
    rt_start();

    if (x != ITERATIONS * NUM_TASKS)
    {
        return 1;
    }
}
