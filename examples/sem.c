#include <rt/rt.h>
#include <rt/sem.h>
#include <rt/sleep.h>
#include <rt/task.h>

static const int n = 10;

static void poster(void *arg)
{
    struct rt_sem *sem = arg;

    for (int i = 1; i <= n; ++i)
    {
        rt_sleep(5);
        rt_sem_post(sem);
    }

    rt_sleep(15);
    rt_sem_post(sem);
}

static volatile bool wait_failed = false;

static void waiter(void *arg)
{
    struct rt_sem *sem = arg;
    for (int i = 1; i <= n; ++i)
    {
        if (!rt_sem_timedwait(sem, 10))
        {
            wait_failed = true;
        }
    }

    if (rt_sem_timedwait(sem, 10))
    {
        wait_failed = true;
    }

    rt_stop();
}

int main(void)
{
    static RT_SEM(sem, 0);
    __attribute__((aligned(STACK_ALIGN))) static char
        poster_stack[TASK_STACK_SIZE],
        waiter_stack[TASK_STACK_SIZE];
    RT_TASK(poster, &sem, poster_stack, 1);
    RT_TASK(waiter, &sem, waiter_stack, 1);
    rt_start();
    if (wait_failed)
    {
        return 1;
    }
}
