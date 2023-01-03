#include <rt/rt.h>
#include <rt/task.h>

static void empty(void)
{
    rt_stop();
}

int main(void)
{
    static char task_stack[TASK_STACK_SIZE]
        __attribute__((aligned(STACK_ALIGN)));
    RT_TASK(empty, task_stack, 1);
    rt_start();
}
