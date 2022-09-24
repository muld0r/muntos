#include <rt/sleep.h>
#include <rt/tick.h>
#include <rt/rt.h>

#include <limits.h>
#include <stdio.h>

static void sleep(void *arg)
{
    int n = 5;
    unsigned long last_wake_tick = 0;
    while (n > 0)
    {
        rt_sleep_periodic(&last_wake_tick, 10);
        printf("wake at tick %lu\n", rt_tick());
        fflush(stdout);
        --n;
    }

    /* Only the second task to finish will call rt_stop. */
    if (!rt_sem_trywait(arg))
    {
        rt_stop();
    }
}

int main(void)
{
    static RT_SEM(stop_sem, 1);
    static char task0_stack[PTHREAD_STACK_MIN], task1_stack[PTHREAD_STACK_MIN];
    RT_TASK(sleep, &stop_sem, task0_stack, 1);
    RT_TASK(sleep, &stop_sem, task1_stack, 1);
    rt_start();
}
