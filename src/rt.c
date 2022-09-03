#include <rt/rt.h>

#include <rt/context.h>
#include <rt/interrupt.h>
#include <rt/sleep.h>
#include <rt/syscall.h>

#include <stdatomic.h>

static RT_LIST(ready_list);
static struct rt_task *_Atomic ready_stack;
static struct rt_task *active_task;

static struct rt_task *task_from_list(struct rt_list *list)
{
    return rt_list_item(list, struct rt_task, list);
}

static void ready_push(struct rt_task *task)
{
    task->ready_next = atomic_load_explicit(&ready_stack, memory_order_relaxed);
    while (!atomic_compare_exchange_weak_explicit(&ready_stack,
                                                  &task->ready_next, task,
                                                  memory_order_release,
                                                  memory_order_relaxed))
    {
    }
}

static struct rt_task *ready_pop(void)
{
    /*
     * Take all elements on the ready stack at once. Tasks added after this step
     * will be on a new stack. This is done to preserve the ordering of tasks
     * added to the ready list. Otherwise, tasks that are added to the stack
     * while the stack is being flushed will be inserted before tasks that were
     * pushed earlier.
     */
    struct rt_task *task =
        atomic_exchange_explicit(&ready_stack, NULL, memory_order_acquire);
    struct rt_list *next = &ready_list;
    while (task)
    {
        /*
         * Add elements to the ready list in the reverse order that they were
         * popped from the stack.
         */
        rt_list_insert_before(&task->list, next);
        next = &task->list;
        task = task->ready_next;
    }

    // TODO: deal with different priorities
    if (rt_list_is_empty(&ready_list))
    {
        return NULL;
    }
    return task_from_list(rt_list_pop_front(&ready_list));
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
