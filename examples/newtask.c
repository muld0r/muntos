#include <rt/rt.h>
#include <rt/task.h>
#include <rt/log.h>

static char task_stacks[2][TASK_STACK_SIZE]
    __attribute__((aligned(STACK_ALIGN)));
static struct rt_task tasks[2];

#define N 100

static void fn(uintptr_t arg)
{
    rt_logf("hello from %u\n", (unsigned)arg);

    if (arg == N)
    {
        rt_stop();
    }

    /* Create a new task in the unused slot with one lower priority, so it will
     * only run once this task has exited, at which point the current slot can be
     * used for a new task. */
    ++arg;
    uintptr_t task_index = arg & 1;
    rt_task_init_arg(&tasks[task_index], fn, arg, "fn", N - (unsigned)arg,
                     task_stacks[task_index], TASK_STACK_SIZE);
}

int main(void)
{
    rt_task_init_arg(&tasks[0], fn, 0, "fn", N, task_stacks[0],
                     TASK_STACK_SIZE);
    rt_start();
}
