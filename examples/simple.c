#include <rt/rt.h>
#include <rt/sem.h>
#include <rt/task.h>

static void simple(void *arg)
{
    (void)arg;
    for (int i = 0; i < 100; ++i)
    {
        rt_yield();
    }

    /* Only the second task to finish will call rt_stop. */
    static RT_SEM(stop_sem, 1);
    if (!rt_sem_trywait(&stop_sem))
    {
        rt_stop();
    }
}

int main(void)
{
    __attribute__((aligned(STACK_ALIGN))) static char stack0[TASK_STACK_SIZE],
        stack1[TASK_STACK_SIZE];
    RT_TASK(simple, NULL, stack0, 1);
    RT_TASK(simple, NULL, stack1, 1);
    rt_start();
}
