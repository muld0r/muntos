#include <rt/critical.h>
#include <rt/sem.h>
#include <rt/rt.h>

#include <limits.h>
#include <stdio.h>

static const int n = 1000;
static RT_SEM(sem, 0);

static void post_fn(void)
{
    for (int i = 1; i <= n; ++i)
    {
        rt_sem_post(&sem);
    }
}

static void wait_fn(void)
{
    for (int i = 1; i <= n; ++i)
    {
        rt_sem_wait(&sem);
        rt_critical_begin();
        printf("sem wait #%d\n", i);
        fflush(stdout);
        rt_critical_end();
    }
    rt_stop();
}

int main(void)
{
    static char poster_stack[PTHREAD_STACK_MIN], waiter_stack[PTHREAD_STACK_MIN];
    static const struct rt_task_config poster_cfg = {
        .fn = post_fn,
        .stack = poster_stack,
        .stack_size = sizeof(poster_stack),
        .name = "poster",
        .priority = 1,
    };
    static const struct rt_task_config waiter_cfg = {
        .fn = wait_fn,
        .stack = waiter_stack,
        .stack_size = sizeof(waiter_stack),
        .name = "waiter",
        .priority = 1,
    };
    static struct rt_task poster, waiter;
    rt_task_init(&poster, &poster_cfg);
    rt_task_init(&waiter, &waiter_cfg);

    rt_start();
}
