#include <rt/rt.h>
#include <rt/sem.h>

#include <limits.h>
#include <stdio.h>

static const int n = 10;
static RT_SEM(sem, 0);

static void poster(void)
{
    for (int i = 1; i <= n; ++i)
    {
        rt_sem_post(&sem);
    }
}

static void waiter(void)
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
    RT_TASK(poster, poster_stack, 1);
    RT_TASK(waiter, waiter_stack, 1);
    rt_start();
}
