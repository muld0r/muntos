#include <rt/sleep.h>

#include <rt/tick.h>

#include <stdio.h>

/*
 * These globals may only be manipulated in the system call handler.
 */
static RT_LIST(sleep_list);
static unsigned long woken_tick;
static unsigned long next_wake_tick;

void rt_sleep(unsigned long ticks)
{
    rt_self()->syscall_args[0] = ticks;
    rt_syscall(RT_SYSCALL_SLEEP);
}

void rt_sleep_periodic(unsigned long *last_wake_tick, unsigned long period)
{
    unsigned long *args = rt_self()->syscall_args;
    args[0] = *last_wake_tick;
    args[1] = period;
    *last_wake_tick += period;
    rt_syscall(RT_SYSCALL_SLEEP_PERIODIC);
}

static void go_to_sleep(unsigned long wake_tick)
{
    const unsigned long ticks_until_wake = wake_tick - woken_tick;

    struct rt_list *node;
    rt_list_for_each(node, &sleep_list)
    {
        const struct rt_task *const sleeping_task =
            rt_list_item(node, struct rt_task, list);
        if (ticks_until_wake < (sleeping_task->syscall_args[0] - woken_tick))
        {
            break;
        }
    }

    rt_self()->syscall_args[0] = wake_tick;
    rt_list_insert_before(&rt_self()->list, node);
    if (rt_list_front(&sleep_list) == &rt_self()->list)
    {
        next_wake_tick = wake_tick;
    }
}

void rt_sleep_syscall(void)
{
    const unsigned long ticks = rt_self()->syscall_args[0];
    /* Only check for 0 ticks in the syscall so that rt_sleep(0) becomes a
     * synonym for rt_yield(). */
    if (ticks > 0)
    {
        go_to_sleep(woken_tick + ticks);
    }
}

void rt_sleep_periodic_syscall(void)
{
    const unsigned long *args = rt_self()->syscall_args;
    const unsigned long last_wake_tick = args[0];
    const unsigned long period = args[1];
    const unsigned long ticks_since_last_wake = woken_tick - last_wake_tick;
    /* If there have been at least as many ticks as the period since the last
     * wake, then the desired wake up tick has already occurred. */
    if (ticks_since_last_wake < period)
    {
        go_to_sleep(last_wake_tick + period);
    }
}

void rt_sleep_check(void)
{
    while (woken_tick < rt_tick())
    {
        ++woken_tick;
        if (woken_tick != next_wake_tick)
        {
            break;
        }

        while (!rt_list_is_empty(&sleep_list))
        {
            struct rt_list *node = rt_list_front(&sleep_list);
            struct rt_task *const sleeping_task =
                rt_list_item(node, struct rt_task, list);
            if (sleeping_task->syscall_args[0] != woken_tick)
            {
                /*
                 * Tasks are ordered by when they should wake up, so if we
                 * reach a task that should still be sleeping, we are done
                 * scanning tasks. This task will be the next to wake, unless
                 * another task goes to sleep later, with an earlier wake tick.
                 */
                next_wake_tick = sleeping_task->syscall_args[0];
                break;
            }
            rt_list_remove(node);
            rt_task_ready(sleeping_task);
        }
    }
}
