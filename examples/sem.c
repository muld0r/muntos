#include <rt/rt.h>
#include <rt/sem.h>
#include <rt/sleep.h>
#include <rt/task.h>

static const int n = 10;
static RT_SEM(sem, 0);

static void poster(uintptr_t arg)
{
    (void)arg;

    for (int i = 1; i <= n; ++i)
    {
        rt_sleep(5);
        rt_sem_post(&sem);
    }

    rt_sleep(15);
    rt_sem_post(&sem);
}

static volatile bool wait_failed = false;

static void waiter(uintptr_t arg)
{
    (void)arg;

    for (int i = 1; i <= n; ++i)
    {
        if (!rt_sem_timedwait(&sem, 10))
        {
            wait_failed = true;
        }
    }

    if (rt_sem_timedwait(&sem, 10))
    {
        wait_failed = true;
    }

    rt_stop();
}

int main(void)
{
    static char task_stacks[2][TASK_STACK_SIZE]
        __attribute__((aligned(STACK_ALIGN)));
    RT_TASK(poster, task_stacks[0], 1);
    RT_TASK(waiter, task_stacks[1], 1);
    rt_start();
    if (wait_failed)
    {
        return 1;
    }
}
