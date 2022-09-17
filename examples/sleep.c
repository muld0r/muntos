#include <rt/sleep.h>
#include <rt/tick.h>
#include <rt/rt.h>

#include <limits.h>
#include <stdio.h>

static void sleep_fn(void)
{
    int n = 5;
    unsigned long last_wake_tick = 0;
    while (n > 0)
    {
        printf("%s %d, tick %lu\n", rt_self()->name, n, rt_tick());
        fflush(stdout);
        rt_sleep_periodic(&last_wake_tick, 100);
        --n;
    }
    rt_stop();
}

int main(void)
{
    static char task0_stack[PTHREAD_STACK_MIN], task1_stack[PTHREAD_STACK_MIN];
    static struct rt_task task0, task1;
    rt_task_init(&task0, sleep_fn, task0_stack, sizeof task0_stack, "task0",
                  1);
    rt_task_init(&task1, sleep_fn, task1_stack, sizeof task1_stack, "task1",
                  1);
    rt_start();
}
