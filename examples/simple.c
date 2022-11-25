#include <rt/rt.h>
#include <rt/sem.h>
#include <rt/task.h>

static void simple(void *arg)
{
    (void)arg;
    for (int i = 0; i < 100; ++i)
    {
        rt_sched();
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
    static char task_stacks[2][TASK_STACK_SIZE]
        __attribute__((aligned(STACK_ALIGN)));
    RT_TASK(simple, NULL, task_stacks[0], 1);
    RT_TASK(simple, NULL, task_stacks[1], 1);
    rt_start();
}
