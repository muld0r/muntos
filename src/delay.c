#include <rt/delay.h>

#include <rt/critical.h>
#include <rt/rt.h>

static LIST_HEAD(delay_list);

static void delay_until(rt_tick_t wake_tick, rt_tick_t max_delay)
{
    const rt_tick_t ticks_until_wake = wake_tick - rt_tick_count();
    if (0 < ticks_until_wake && ticks_until_wake <= max_delay)
    {
        rt_critical_begin();
        struct list *delay_insert;
        for (delay_insert = list_front(&delay_list);
             delay_insert != &delay_list; delay_insert = delay_insert->next)
        {
            const struct rt_task *task =
                list_item(delay_insert, struct rt_task, list);
            if (task->wake_tick - rt_tick_count() > ticks_until_wake)
            {
                break;
            }
        }
        rt_task_self()->wake_tick = wake_tick;
        list_add_tail(delay_insert, &rt_task_self()->list);
        rt_critical_end();
        rt_task_suspend(rt_task_self());
    }
}

void rt_delay(rt_tick_t delay)
{
    delay_until(rt_tick_count() + delay, delay);
}

void rt_delay_periodic(rt_tick_t *last_wake_tick, rt_tick_t period)
{
    delay_until(*last_wake_tick += period, period);
}

void rt_delay_wake_tasks(void)
{
    rt_tick_t current_tick = rt_tick_count();
    for (struct rt_task *task =
             list_item(list_front(&delay_list), struct rt_task, list);
         (task != NULL) && (current_tick == task->wake_tick);
         task = list_item(list_front(&delay_list), struct rt_task, list))
    {
        rt_task_resume(task);
    }
}
