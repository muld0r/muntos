#include <muntos/muntos.h>
#include <muntos/sem.h>
#include <muntos/sleep.h>
#include <muntos/task.h>

static const int n = 10;
static RT_SEM(sem, 0);

static void poster(void)
{
    rt_task_drop_privilege();
    for (int i = 1; i <= n; ++i)
    {
        rt_sleep(5);
        rt_sem_post(&sem);
    }

    rt_sleep(15);
    rt_sem_post(&sem);
}

static volatile bool wait_failed = false;

static void waiter(void)
{
    rt_task_drop_privilege();
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
    RT_TASK(poster, RT_STACK_MIN, 1);
    RT_TASK(waiter, RT_STACK_MIN, 1);
    rt_start();
    if (wait_failed)
    {
        return 1;
    }
}
