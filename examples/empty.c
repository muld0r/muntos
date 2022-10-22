#include <rt/rt.h>
#include <rt/task.h>

static void empty(void *arg)
{
    (void)arg;
    rt_stop();
}

int main(void)
{
    __attribute__((aligned(STACK_ALIGN))) static char stack[TASK_STACK_SIZE];
    RT_TASK(empty, NULL, stack, 1);
    rt_start();
}
