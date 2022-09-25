#include <rt/sem.h>
#include <rt/rt.h>

static void simple(void *arg)
{
    int n = 100;
    while (n > 0)
    {
        rt_yield();
        --n;
    }

    /* Only the second task to finish will call rt_stop. */
    if (!rt_sem_trywait(arg))
    {
        rt_stop();
    }
}

int main(void)
{
    static RT_SEM(stop_sem, 1);
    __attribute__((aligned(STACK_ALIGN))) static char stack0[TASK_STACK_SIZE],
        stack1[TASK_STACK_SIZE];
    RT_TASK(simple, &stop_sem, stack0, 1);
    RT_TASK(simple, &stop_sem, stack1, 1);
    rt_start();
}
