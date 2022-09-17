#include <rt/rt.h>

#include <limits.h>

static void empty_fn(void)
{
    rt_stop();
}

int main(void)
{
    static char stack[PTHREAD_STACK_MIN];
    static struct rt_task task;
    rt_task_init(&task, empty_fn, stack, sizeof stack, "empty", 1);
    rt_start();
}
