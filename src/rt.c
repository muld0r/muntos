#include <rt/rt.h>

#include <rt/context.h>
#include <rt/critical.h>
#include <rt/delay.h>
#include <rt/port.h>
#include <rt/sem.h>
#include <rt/syscall.h>

#include <stdio.h>

static LIST_HEAD(ready_list);
static struct rt_task *active_task = NULL;

struct rt_task *rt_task_self(void)
{
    return active_task;
}

void rt_yield(void)
{
    rt_syscall(RT_SYSCALL_YIELD);
#if 0
    rt_critical_begin();
    struct rt_task *task = rt_task_self();
    rt_task_suspend(task);
    list_add_tail(&ready_list, &task->list);
    rt_critical_end();
#endif
}

void rt_sched(void)
{
    struct rt_task *next_task = list_item(list_front(&ready_list), struct rt_task, list);
    if (next_task != active_task)
    {
        if (active_task != NULL)
        {
            list_add_tail(&ready_list, &active_task->list);
        }
        list_remove(&next_task->list);
        active_task = next_task;
    }
}

void rt_task_suspend(struct rt_task *task)
{
    rt_critical_begin();
    list_remove(&task->list);
    if ((task == active_task) && !list_empty(&ready_list))
    {
        // TODO: deal with different priorities
        active_task = list_item(list_front(&ready_list), struct rt_task, list);
        list_remove(&active_task->list);
        printf("suspending %s, resuming %s\n", task->cfg.name, active_task->cfg.name);
        fflush(stdout);
        rt_context_save(task->ctx);
        rt_context_load(active_task->ctx);
    }
    rt_critical_end();
}

void rt_task_exit(struct rt_task *task)
{
    (void)task;
#if 0
    rt_task_suspend(task);
    rt_context_destroy(task->ctx);
#endif
}

void rt_task_resume(struct rt_task *task)
{
    // TODO
    (void)task;
#if 0
    rt_critical_begin();
    if (task != active_task)
    {
        list_remove(&task->list);
        list_remove(&task->event_list);
        list_add_tail(&ready_list, &task->list);
        // TODO: deal with different priorities
    }
    rt_critical_end();
#endif
}

void rt_task_init(struct rt_task *task, const struct rt_task_config *cfg)
{
    list_node_init(&task->list);
    list_node_init(&task->event_list);
    task->cfg = *cfg;
    task->wake_tick = 0;
    task->ctx = rt_context_create(cfg->stack, cfg->stack_size, task->cfg.fn);
    list_add_tail(&ready_list, &task->list);
}

void rt_start(void)
{
    rt_port_start();
}

void rt_stop(void)
{
    rt_port_stop();
}
