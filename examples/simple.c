#include <muntos/muntos.h>
#include <muntos/task.h>

static void simple(uintptr_t arg)
{
    rt_task_drop_privilege();
    for (int i = 0; i < 100; ++i)
    {
        rt_task_yield();
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
