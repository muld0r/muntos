#include <rt/sem.h>
#include <rt/rt.h>

#include <stdatomic.h>

static void simple(void *arg)
{
    atomic_int *n = arg;
    while (atomic_fetch_sub(n, 1) > 0)
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
    static atomic_int n = 200;
    __attribute__((aligned(STACK_ALIGN))) static char stack0[TASK_STACK_SIZE],
        stack1[TASK_STACK_SIZE];
    RT_TASK(simple, &n, stack0, 1);
    RT_TASK(simple, &n, stack1, 1);
    rt_start();
}
