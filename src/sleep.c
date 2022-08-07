#include <rt/sleep.h>

#include <rt/critical.h>
#include <rt/rt.h>

#include <stdio.h>

static RT_LIST(sleep_list);

void rt_sleep(unsigned long ticks)
{
    rt_critical_begin();
    rt_sleep_until(rt_tick() + ticks);
    rt_critical_end();
}

void rt_sleep_periodic(unsigned long *last_wake_tick, unsigned long period)
{
    rt_critical_begin();
    const unsigned long current_tick = rt_tick();
    const unsigned long ticks_since_last_wake = current_tick - *last_wake_tick;
    *last_wake_tick += period;
    if (ticks_since_last_wake < period)
    {
        rt_sleep_until(*last_wake_tick);
    }
    rt_critical_end();
}

void rt_sleep_until(unsigned long wake_tick)
{
    // TODO: only mask ticks
    rt_critical_begin();
    const unsigned long current_tick = rt_tick();
    const unsigned long ticks_until_wake = wake_tick - current_tick;

    struct rt_list *node;
    rt_list_for_each(node, &sleep_list)
    {
        const struct rt_task *const sleeping_task =
            rt_list_item(node, struct rt_task, list);
        if (ticks_until_wake < (sleeping_task->wake_tick - current_tick))
        {
            break;
        }
    }

#ifdef RT_LOG
    printf("sleeping %s until tick %lu\n", rt_task_self()->cfg.name,
           wake_tick);
    fflush(stdout);
#endif
    rt_list_insert_before(&rt_task_self()->list, node);
    rt_task_self()->wake_tick = wake_tick;
    rt_yield();
    rt_critical_end();
}

void rt_sleep_check(void)
{
    // TODO: store and check the first wake tick without looking at the list if
    // not needed
    const unsigned long current_tick = rt_tick();
    while (!rt_list_is_empty(&sleep_list))
    {
        struct rt_list *node = rt_list_front(&sleep_list);
        struct rt_task *const sleeping_task =
            rt_list_item(node, struct rt_task, list);
        if (sleeping_task->wake_tick != current_tick)
        {
            /*
             * Tasks are ordered by when they should wake up, so if we reach a
             * task that should still be sleeping, we are done.
             */
            break;
        }
        rt_task_resume(sleeping_task);
    }
}
