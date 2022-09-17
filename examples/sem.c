#include <rt/rt.h>
#include <rt/sem.h>

#include <limits.h>
#include <stdio.h>

static const int n = 10;
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
        printf("sem wait #%d\n", i);
        fflush(stdout);
    }
    rt_stop();
}

int main(void)
{
    static char poster_stack[PTHREAD_STACK_MIN],
        waiter_stack[PTHREAD_STACK_MIN];
    static struct rt_task poster, waiter;
    rt_task_init(&poster, post_fn, poster_stack, sizeof poster_stack, "poster",
                  1);
    rt_task_init(&waiter, wait_fn, waiter_stack, sizeof waiter_stack, "waiter",
                  1);
    rt_start();
}
