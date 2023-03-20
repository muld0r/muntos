#include <rt/log.h>
#include <rt/once.h>
#include <rt/rt.h>
#include <rt/task.h>

#define ITERATIONS 10000UL

static RT_ONCE(once);
static RT_SEM(sem, 0);

static volatile unsigned long x = 0;

static void fn(void)
{
    rt_logf("once: %s\n", rt_task_name());
    ++x;
}

static void oncer(void)
{
    for (unsigned long i = 0; i < ITERATIONS; ++i)
    {
        rt_once(&once, fn);
        rt_sem_wait(&sem);
    }
}

static void oncer_reset(void)
{
    for (unsigned long i = 0; i < ITERATIONS; ++i)
    {
        rt_once(&once, fn);
        rt_atomic_store_explicit(&once.done, 0, memory_order_release);
        rt_sem_post(&sem);
    }
    rt_stop();
}

int main(void)
{
    RT_TASK(oncer, RT_STACK_MIN, 1);
    RT_TASK(oncer_reset, RT_STACK_MIN, 1);
    rt_start();

    if (x != ITERATIONS)
    {
        return 1;
    }
}
