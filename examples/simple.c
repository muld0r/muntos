#include <rt/sleep.h>
#include <rt/tick.h>
#include <rt/rt.h>

#include <limits.h>
#include <stdio.h>
#include <string.h>

static void simple_fn(void)
{
    int n = 100;
    while (n > 0)
    {
        rt_yield();
        --n;
    }
    printf("%s finished\n", rt_self()->name);
    fflush(stdout);
    if (strcmp(rt_self()->name, "task0") == 0)
    {
        rt_sleep(500);
        rt_stop();
    }
}

int main(void)
{
    static char stack0[PTHREAD_STACK_MIN], stack1[PTHREAD_STACK_MIN];
    static struct rt_task task0, task1;
    rt_task_init(&task0, simple_fn, stack0, sizeof stack0, "task0", 1);
    rt_task_init(&task1, simple_fn, stack1, sizeof stack1, "task1", 1);
    rt_start();
}
