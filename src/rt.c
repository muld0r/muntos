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

static struct rt_task *ready_pop(void)
{
    struct rt_task *next_task = list_item(list_front(&ready_list), struct rt_task, list);
    if (next_task)
    {
        list_remove(&next_task->list);
    }
    return next_task;
}

static void ready_push(struct rt_task *task)
{
    list_add_tail(&ready_list, &task->list);
}

struct rt_task *rt_task_self(void)
{
    return active_task;
}

void rt_yield(void)
{
    printf("rt_yield from %s\n", active_task->cfg.name);
    fflush(stdout);
    rt_syscall(RT_SYSCALL_YIELD);
}

void rt_sched(void)
{
    struct rt_task *next_task = ready_pop();
    if (!next_task)
    {
        printf("no new task to schedule\n");
        fflush(stdout);
        return;
    }

    printf("next task is %s\n", next_task->cfg.name);
    fflush(stdout);

    if (active_task)
    {
        printf("saving context of %s\n", active_task->cfg.name);
        fflush(stdout);
        rt_context_save(active_task->ctx);
        ready_push(active_task);
    }
    else
    {
        printf("no active task\n");
        fflush(stdout);
    }

    active_task = next_task;
    printf("loading context of %s\n", active_task->cfg.name);
    fflush(stdout);
    rt_context_load(active_task->ctx);
}

void rt_task_suspend(struct rt_task *task)
{
#if 0
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
#endif
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
    ready_push(task);
}

void rt_start(void)
{
    rt_port_start();
}

void rt_stop(void)
{
    rt_port_stop();
}
