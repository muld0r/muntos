#include <rt/critical.h>
#include <rt/tick.h>
#include <rt/rt.h>

#include <limits.h>
#include <stdio.h>

static void simple_fn(void)
{
    int n = 10000;
    while (n > 0)
    {
        rt_critical_begin();
        printf("%s %d, tick %lu\n", rt_task_self()->name, n, rt_tick());
        fflush(stdout);
        rt_critical_end();
        --n;
    }
    rt_stop();
}

int main(void)
{
    static char stack0[PTHREAD_STACK_MIN], stack1[PTHREAD_STACK_MIN];
    static RT_TASK(task0, simple_fn, stack0, 1);
    static RT_TASK(task1, simple_fn, stack1, 1);
    rt_task_launch(&task0);
    rt_task_launch(&task1);

    rt_start();
}
