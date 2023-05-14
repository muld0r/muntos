#include <rt/notify.h>
#include <rt/rt.h>
#include <rt/sleep.h>
#include <rt/task.h>

static const int n = 10;
static struct rt_notify note;
static RT_NOTIFY(note, 0);

static void notifier(void)
{
    rt_task_drop_privilege();
    for (int i = 1; i <= n; ++i)
    {
        rt_sleep(5);
        rt_notify_or(&note, 1);
    }

    rt_sleep(15);
    rt_notify(&note);
}

static volatile bool wait_failed = false;

static void waiter(void)
{
    rt_task_drop_privilege();
    uint32_t value;

    for (int i = 1; i <= n; ++i)
    {
        if (!rt_notify_timedwait_clear(&note, 1, &value, 10))
        {
            wait_failed = true;
        }
    }

    if (rt_notify_timedwait(&note, &value, 10))
    {
        wait_failed = true;
    }

    rt_stop();
}

int main(void)
{
    RT_TASK(notifier, RT_STACK_MIN, 1);
    RT_TASK(waiter, RT_STACK_MIN, 1);

    rt_start();
    if (wait_failed)
    {
        return 1;
    }
}
