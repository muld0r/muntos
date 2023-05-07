#include <rt/rt.h>
#include <rt/task.h>

static volatile float v;

static void f(uintptr_t arg)
{
    rt_task_drop_privilege();
    rt_task_enable_fp();

    float x = 0.0f;
    float a = (float)arg;
    for (;;)
    {
        x += a;
        v = x;
        rt_task_yield();
    }
}

int main(void)
{
    RT_TASK_ARG(f, 1, 2*RT_STACK_MIN, 1);
    RT_TASK_ARG(f, 2, 2*RT_STACK_MIN, 1);
    rt_start();
}
