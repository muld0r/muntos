#include <rt/rt.h>

#include <rt/context.h>
#include <rt/interrupt.h>
#include <rt/sleep.h>
#include <rt/syscall.h>
#include <rt/tick.h>

#include <stdatomic.h>

static RT_LIST(ready_list);

static struct rt_syscall_record *_Atomic pending_syscalls;

static struct rt_task *active_task;

static struct rt_task *task_from_list(struct rt_list *list)
{
    return rt_list_item(list, struct rt_task, list);
}

static struct rt_task *ready_pop(void)
{
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

static void task_syscall(struct rt_task *task, enum rt_syscall syscall)
{
    task->syscall_record.syscall = syscall;
    rt_syscall_push(&task->syscall_record);
    rt_syscall_post();
}

void rt_yield(void)
{
    task_syscall(active_task, RT_SYSCALL_YIELD);
}

static struct atomic_flag sched_pending = ATOMIC_FLAG_INIT;

void rt_sched(void)
{
    static struct rt_syscall_record sched = {
        .next = NULL,
        .syscall = RT_SYSCALL_SCHED,
    };

    if (!atomic_flag_test_and_set_explicit(&sched_pending,
                                           memory_order_acquire))
    {
        rt_syscall_push(&sched);
        rt_syscall_post();
    }
}

void rt_exit(void)
{
    task_syscall(active_task, RT_SYSCALL_EXIT);
}

void rt_task_ready(struct rt_task *task)
{
    task_syscall(task, RT_SYSCALL_READY);
}

static void sched(void)
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
            rt_list_push_back(&ready_list, &prev_task->list);
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

/*
 * These globals may only be manipulated in the system call handler.
 */
static RT_LIST(sleep_list);
static unsigned long woken_tick;
static unsigned long next_wake_tick;

static void go_to_sleep(unsigned long wake_tick)
{
    const unsigned long ticks_until_wake = wake_tick - woken_tick;

    struct rt_list *node;
    rt_list_for_each(node, &sleep_list)
    {
        const struct rt_task *const sleeping_task =
            rt_list_item(node, struct rt_task, list);
        if (ticks_until_wake <
            (sleeping_task->syscall_args.wake_tick - woken_tick))
        {
            break;
        }
    }

    rt_self()->syscall_args.wake_tick = wake_tick;
    rt_list_insert_before(&rt_self()->list, node);
    if (rt_list_front(&sleep_list) == &rt_self()->list)
    {
        next_wake_tick = wake_tick;
    }
}

static void sleep_syscall(void)
{
    const unsigned long ticks = rt_self()->syscall_args.sleep_ticks;
    /* Only check for 0 ticks in the syscall so that rt_sleep(0) becomes a
     * synonym for rt_yield(). */
    if (ticks > 0)
    {
        go_to_sleep(woken_tick + ticks);
    }
}

static void sleep_periodic_syscall(void)
{
    struct rt_task *self = rt_self();
    const unsigned long last_wake_tick =
        self->syscall_args.sleep_periodic.last_wake_tick;
    const unsigned long period = self->syscall_args.sleep_periodic.period;
    const unsigned long ticks_since_last_wake = woken_tick - last_wake_tick;
    /* If there have been at least as many ticks as the period since the last
     * wake, then the desired wake up tick has already occurred. */
    if (ticks_since_last_wake < period)
    {
        go_to_sleep(last_wake_tick + period);
    }
}

static void tick_syscall(void)
{
    /*
     * The tick counter moves independently of tick syscall, so process all
     * ticks until caught up.
     */
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
            if (sleeping_task->syscall_args.wake_tick != woken_tick)
            {
                /*
                 * Tasks are ordered by when they should wake up, so if we
                 * reach a task that should still be sleeping, we are done
                 * scanning tasks. This task will be the next to wake, unless
                 * another task goes to sleep later, with an earlier wake tick.
                 */
                next_wake_tick = sleeping_task->syscall_args.wake_tick;
                break;
            }
            rt_list_remove(node);
            rt_list_push_back(&ready_list, node);
        }
    }
}

static atomic_ulong rt_ticks;
static struct atomic_flag tick_pending = ATOMIC_FLAG_INIT;

void rt_tick_advance(void)
{
    atomic_fetch_add_explicit(&rt_ticks, 1, memory_order_relaxed);

    static struct rt_syscall_record tick_syscall_record = {
        .next = NULL,
        .syscall = RT_SYSCALL_TICK,
    };

    if (!atomic_flag_test_and_set_explicit(&tick_pending, memory_order_acquire))
    {
        rt_syscall_push(&tick_syscall_record);
        rt_syscall_post();
    }
}

unsigned long rt_tick(void)
{
    return atomic_load_explicit(&rt_ticks, memory_order_relaxed);
}

void rt_syscall_push(struct rt_syscall_record *syscall_record)
{
    syscall_record->next =
        atomic_load_explicit(&pending_syscalls, memory_order_relaxed);
    while (!atomic_compare_exchange_weak_explicit(&pending_syscalls,
                                                  &syscall_record->next,
                                                  syscall_record,
                                                  memory_order_release,
                                                  memory_order_relaxed))
    {
    }
}

static struct rt_task *
task_from_syscall_record(struct rt_syscall_record *record)
{
    return rt_container_of(record, struct rt_task, syscall_record);
}

void rt_syscall_handler(void)
{
    /*
     * Take all elements on the pending syscall stack at once. Syscalls added
     * after this step will be on a new stack. This is done to preserve the
     * ordering of syscalls invoked. Otherwise, syscalls that are added to the
     * stack while the stack is being flushed will be handled before tasks that
     * were pushed earlier.
     */
    struct rt_syscall_record *syscall_record =
        atomic_exchange_explicit(&pending_syscalls, NULL, memory_order_acquire);
    struct rt_list *ready_next = &ready_list;
    while (syscall_record)
    {
        switch (syscall_record->syscall)
        {
        case RT_SYSCALL_NONE:
            break;
        case RT_SYSCALL_SCHED:
            atomic_flag_clear_explicit(&sched_pending, memory_order_release);
            break;
        case RT_SYSCALL_TICK:
            atomic_flag_clear_explicit(&tick_pending, memory_order_release);
            tick_syscall();
            break;
        case RT_SYSCALL_READY:
        {
            /*
             * Add elements to the ready list in the reverse order that their
             * READY syscalls are popped from the stack.
             */
            struct rt_task *task = task_from_syscall_record(syscall_record);
            rt_list_insert_before(&task->list, ready_next);
            ready_next = &task->list;
            syscall_record->syscall = RT_SYSCALL_NONE;
            break;
        }
        case RT_SYSCALL_YIELD:
            break;
        case RT_SYSCALL_SLEEP:
            sleep_syscall();
            break;
        case RT_SYSCALL_SLEEP_PERIODIC:
            sleep_periodic_syscall();
            break;
        case RT_SYSCALL_EXIT:
            rt_context_destroy(active_task->ctx);
            active_task = NULL;
            break;
        }
        syscall_record = syscall_record->next;
    }

    sched();
}

void rt_task_start(struct rt_task *task)
{
    task->ctx = rt_context_create(task->stack, task->stack_size, task->fn);
    rt_list_push_back(&ready_list, &task->list);
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
