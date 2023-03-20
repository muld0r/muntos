#include <rt/rt.h>
#include <rt/task.h>

static void empty(void)
{
    rt_stop();
}

int main(void)
{
    RT_TASK(empty, RT_STACK_MIN, 1);
    rt_start();
}
