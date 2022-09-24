#include <rt/sleep.h>
#include <rt/tick.h>
#include <rt/rt.h>
#include <rt/sem.h>

#include <limits.h>
#include <stdio.h>
#include <string.h>

static void simple(void *arg)
{
    int n = 100;
    while (n > 0)
    {
        rt_yield();
        --n;
    }

    /* Only the second task to finish will call rt_stop. */
    if (!rt_sem_trywait(arg))
    {
        printf("finished\n");
        fflush(stdout);
        rt_stop();
    }
}

int main(void)
{
    static RT_SEM(stop_sem, 1);
    static char stack0[PTHREAD_STACK_MIN], stack1[PTHREAD_STACK_MIN];
    RT_TASK(simple, &stop_sem, stack0, 1);
    RT_TASK(simple, &stop_sem, stack1, 1);
    rt_start();
}
