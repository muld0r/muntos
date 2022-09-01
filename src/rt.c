#include <rt/rt.h>

#include <rt/context.h>
#include <rt/interrupt.h>
#include <rt/syscall.h>

#include <stdio.h>

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
    rt_self()->exiting = true;
    rt_yield();
}

static void yield(void)
{
    struct rt_task *const prev_task = active_task;
    struct rt_task *const next_task = ready_pop();
    const bool still_ready =
        prev_task && !prev_task->exiting && rt_list_is_empty(&prev_task->list);

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

void rt_syscall_run(enum rt_syscall syscall)
{
    switch (syscall)
    {
    case RT_SYSCALL_NOP:
        break;
    case RT_SYSCALL_YIELD:
        yield();
        break;
    }
}

void rt_task_start(struct rt_task *task)
{
    task->ctx = rt_context_create(task->stack, task->stack_size, task->fn);
    rt_task_ready(task);
}

void rt_task_ready(struct rt_task *task)
{
    ready_push(task);
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
