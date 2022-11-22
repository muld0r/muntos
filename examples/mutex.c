#include <rt/mutex.h>
#include <rt/rt.h>
#include <rt/sem.h>
#include <rt/sleep.h>
#include <rt/task.h>

static RT_MUTEX(mutex);

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

static void increment_lock(void *arg)
{
    unsigned long *x = arg;
    for (unsigned long i = 0; i < ITERATIONS; ++i)
    {
        rt_mutex_lock(&mutex);
        ++*x;
        rt_mutex_unlock(&mutex);
    }
    stop_last();
}

static void increment_trylock(void *arg)
{
    unsigned long *x = arg;
    for (unsigned long i = 0; i < ITERATIONS; ++i)
    {
        while (!rt_mutex_trylock(&mutex))
        {
            rt_sleep(1);
        }
        ++*x;
        rt_mutex_unlock(&mutex);
    }
    stop_last();
}

static void increment_timedlock(void *arg)
{
    unsigned long *x = arg;
    for (unsigned long i = 0; i < ITERATIONS; ++i)
    {
        while (!rt_mutex_timedlock(&mutex, 1))
        {
        }
        ++*x;
        rt_mutex_unlock(&mutex);
    }
    stop_last();
}

int main(void)
{
    unsigned long x = 0;
    static char task_stacks[NUM_TASKS][TASK_STACK_SIZE]
        __attribute__((aligned(STACK_ALIGN)));
    RT_TASK(increment_lock, &x, task_stacks[0], 1);
    RT_TASK(increment_trylock, &x, task_stacks[1], 1);
    RT_TASK(increment_timedlock, &x, task_stacks[2], 1);
    rt_start();

    if (x != ITERATIONS * NUM_TASKS)
    {
        return 1;
    }
}
