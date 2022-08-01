#include <rt/critical.h>
#include <rt/rt.h>

#include <limits.h>
#include <stdio.h>

static void simple_fn(void)
{
    int n = 1000;
    while (n > 0)
    {
        printf("%s %d, tick %lu\n", rt_task_self()->cfg.name, n,
               rt_tick_count());
        fflush(stdout);
        --n;
        rt_yield();
    }
    rt_stop();
}

int main(void)
{
    static char task_stack[PTHREAD_STACK_MIN];
    static const struct rt_task_config task_cfg = {
        .fn = simple_fn,
        .stack = task_stack,
        .stack_size = sizeof(task_stack),
        .name = "task",
        .priority = 1,
    };
    static struct rt_task task;
    rt_task_init(&task, &task_cfg);

    rt_start();
}
