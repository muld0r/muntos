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
    static char task_stacks[2][TASK_STACK_SIZE]
        __attribute__((aligned(STACK_ALIGN)));
    RT_TASK_ARG(simple, 0, task_stacks[0], 1);
    RT_TASK_ARG(simple, 1, task_stacks[1], 1);
    rt_start();
}
