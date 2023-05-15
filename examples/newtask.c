#include <muntos/log.h>
#include <muntos/muntos.h>
#include <muntos/task.h>

RT_STACKS(task_stacks, RT_STACK_MIN, 2);
static struct rt_task tasks[2];

#define N 100

static void fn(uintptr_t arg)
{
    rt_task_drop_privilege();
    rt_logf("hello from %u\n", (unsigned)arg);

    if (arg == N)
    {
        rt_stop();
    }

    /* Create a new task in the unused slot with one lower priority, so it will
     * only run once this task has exited, at which point the current slot can
     * be used for a new task. */
    ++arg;
    uintptr_t task_index = arg & 1;
    rt_task_init_arg(&tasks[task_index], fn, arg, "fn", N - (unsigned)arg,
                     task_stacks[task_index], RT_STACK_MIN);
}

int main(void)
{
    rt_task_init_arg(&tasks[0], fn, 0, "fn", N, task_stacks[0], RT_STACK_MIN);
    rt_start();
}
