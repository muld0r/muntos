#include <rt/rt.h>
#include <rt/sem.h>

#include <limits.h>
#include <stdio.h>

static const int n = 10;

static void poster(void *arg)
{
    struct rt_sem *sem = arg;
    for (int i = 1; i <= n; ++i)
    {
        rt_sem_post(sem);
    }
}

static void waiter(void *arg)
{
    struct rt_sem *sem = arg;
    for (int i = 1; i <= n; ++i)
    {
        rt_sem_wait(sem);
        printf("sem wait #%d\n", i);
        fflush(stdout);
    }
    rt_stop();
}

int main(void)
{
    static RT_SEM(sem, 0);
    static char poster_stack[PTHREAD_STACK_MIN],
        waiter_stack[PTHREAD_STACK_MIN];
    RT_TASK(poster, &sem, poster_stack, 1);
    RT_TASK(waiter, &sem, waiter_stack, 1);
    rt_start();
}
