#include <rt/rt.h>

#include <rt/context.h>
#include <rt/critical.h>
#include <rt/interrupt.h>
#include <rt/sleep.h>
#include <rt/syscall.h>

static RT_LIST(ready_list);
static struct rt_task *active_task;

static struct rt_task *task_from_list(struct rt_list *list)
{
    return rt_list_item(list, struct rt_task, list);
}

static struct rt_task *ready_pop(void)
{
    if (rt_list_is_empty(&ready_list))
    {
        return NULL;
    }
    struct rt_task *next_task =
        task_from_list(rt_list_pop_front(&ready_list));
    return next_task;
}

static void ready_push(struct rt_task *task)
{
    // TODO: deal with different priorities
    rt_list_push_back(&ready_list, &task->list);
}

struct rt_task *rt_self(void)
{
    return active_task;
}

void rt_yield(void)
{
    rt_syscall(RT_SYSCALL_YIELD);
}

void rt_exit(void)
{
    rt_syscall(RT_SYSCALL_EXIT);
}

static void yield(void)
{
    struct rt_task *const prev_task = active_task;
    struct rt_task *const next_task = ready_pop();
    const bool still_ready = prev_task && rt_list_is_empty(&prev_task->list);

    /*
     * If there is no new task to schedule and the current task is still ready
     * there's nothing to do.
     */
    if (!next_task && still_ready)
    {
        return;
    }

    active_task = next_task;
    if (prev_task)
    {
        rt_context_save(prev_task->ctx);
        /*
         * If the yielding task is still ready, re-add it to the ready list.
         */
        if (still_ready)
        {
            ready_push(prev_task);
        }
    }

    if (active_task)
    {
        rt_context_load(active_task->ctx);
    }
    else
    {
        rt_interrupt_wait();
    }
}

void rt_syscall(enum rt_syscall syscall)
{
    if (active_task)
    {
        active_task->syscall = syscall;
    }
    rt_syscall_post();
}

void rt_syscall_handler(void)
{
    enum rt_syscall syscall = RT_SYSCALL_YIELD;

    if (active_task)
    {
        syscall = active_task->syscall;
        active_task->syscall = RT_SYSCALL_YIELD;
    }

    switch (syscall)
    {
    case RT_SYSCALL_YIELD:
        break;
    case RT_SYSCALL_SLEEP:
        rt_sleep_syscall();
        break;
    case RT_SYSCALL_SLEEP_PERIODIC:
        rt_sleep_periodic_syscall();
        break;
    case RT_SYSCALL_EXIT:
        rt_context_destroy(active_task->ctx);
        active_task = NULL;
        break;
    }

    rt_sleep_check();
    yield();
}

void rt_task_start(struct rt_task *task)
{
    task->ctx = rt_context_create(task->stack, task->stack_size, task->fn);
    rt_task_ready(task);
}

void rt_task_ready(struct rt_task *task)
{
    // TODO: do this without a critical section
    rt_critical_begin();
    ready_push(task);
    rt_critical_end();
}

void rt_exit_all(void)
{
    struct rt_task *task;
    while ((task = ready_pop()))
    {
        rt_context_destroy(task->ctx);
    }

    if (active_task)
    {
        rt_context_destroy(active_task->ctx);
    }
}
