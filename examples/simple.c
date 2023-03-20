#include <rt/rt.h>
#include <rt/task.h>

static void simple(uintptr_t arg)
{
    for (int i = 0; i < 100; ++i)
    {
        rt_sched();
    }

    if (arg == 1)
    {
        rt_stop();
    }
}

int main(void)
{
    RT_TASK_ARG(simple, 0, RT_STACK_MIN, 1);
    RT_TASK_ARG(simple, 1, RT_STACK_MIN, 1);
    rt_start();
}
