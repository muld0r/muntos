#include <rt/mutex.h>
#include <rt/sleep.h>
#include <rt/rt.h>

#include <limits.h>
#include <stdio.h>

static const int n = 50;
static RT_MUTEX(mutex);

static int x;

static void increment(void)
{
    for (int i = 0; i < n; ++i)
    {
        rt_mutex_lock(&mutex);
        ++x;
        rt_mutex_unlock(&mutex);
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
    static char stack0[PTHREAD_STACK_MIN], stack1[PTHREAD_STACK_MIN];
    RT_TASK(increment, stack0, 1);
    RT_TASK(increment, stack1, 1);
    rt_start();

    printf("%d\n", x);
}
