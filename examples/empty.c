#include <rt/rt.h>

#include <limits.h>

static void empty_fn(void)
{
    rt_stop();
}

int main(void)
{
    static char stack[PTHREAD_STACK_MIN];
    static RT_TASK(task, empty_fn, stack, 1);
    rt_task_start(&task);
    rt_start();
}
