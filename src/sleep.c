#include <rt/sleep.h>

#include <rt/critical.h>
#include <rt/rt.h>

#include <stdio.h>

static LIST(sleep_list);

void rt_sleep_until(unsigned long wake_tick)
{
    // TODO: only mask ticks
    rt_critical_begin();
    const unsigned long current_tick = rt_tick();
    const unsigned long ticks_until_wake = wake_tick - current_tick;

    struct list *node;
    list_for_each(node, &sleep_list)
    {
        const struct rt_task *const sleeping_task =
            list_item(node, struct rt_task, list);
        if (ticks_until_wake < (sleeping_task->wake_tick - current_tick))
        {
            break;
        }
    }

#ifdef RT_LOG
    printf("sleeping %s until tick %lu\n", rt_task_self()->cfg.name, wake_tick);
    fflush(stdout);
#endif
    list_insert_before(&rt_task_self()->list, node);
    rt_task_self()->wake_tick = wake_tick;
    rt_yield();
    rt_critical_end();
}

void rt_sleep(unsigned long ticks)
{
    rt_critical_begin();
    rt_sleep_until(rt_tick() + ticks);
    rt_critical_end();
}

void rt_sleep_check(void)
{
    unsigned long current_tick = rt_tick();
    struct list *node;
    list_for_each(node, &sleep_list)
    {
        struct rt_task *const sleeping_task =
            list_item(node, struct rt_task, list);
        if (sleeping_task->wake_tick == current_tick)
        {
            rt_task_resume(sleeping_task);
        }
        else
        {
            /*
             * Tasks are ordered by when they should wake up, so if we reach a
             * task that should still be sleeping, we are done.
             */
            break;
        }
    }
}

#if 0
static void sleep_until(rt_tick_t wake_tick, rt_tick_t max_sleep)
{
    const rt_tick_t ticks_until_wake = wake_tick - rt_tick_count();



    if (0 < ticks_until_wake && ticks_until_wake <= max_sleep)
    {
        rt_critical_begin();
        struct list *sleep_insert;
        for (sleep_insert = list_front(&sleep_list);
             sleep_insert != &sleep_list; sleep_insert = sleep_insert->next)
        {
            const struct rt_task *task =
                list_item(sleep_insert, struct rt_task, list);
            if (task->wake_tick - rt_tick_count() > ticks_until_wake)
            {
                break;
            }
        }
        rt_task_self()->wake_tick = wake_tick;
        list_push_back(sleep_insert, &rt_task_self()->list);
        rt_critical_end();
        rt_task_suspend(rt_task_self());
    }
}

void rt_sleep_periodic(rt_tick_t *last_wake_tick, rt_tick_t period)
{
    sleep_until(*last_wake_tick += period, period);
}
#endif
