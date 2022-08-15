#include <rt/critical.h>
#include <rt/rt.h>
#include <rt/sem.h>

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
    static char poster_stack[PTHREAD_STACK_MIN],
        waiter_stack[PTHREAD_STACK_MIN];
    static RT_TASK(poster, post_fn, poster_stack, 1);
    static RT_TASK(waiter, wait_fn, waiter_stack, 1);
    rt_task_launch(&poster);
    rt_task_launch(&waiter);

    rt_start();
}
