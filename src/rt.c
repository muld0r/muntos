#include <rt/rt.h>

#include <rt/context.h>
#include <rt/sleep.h>
#include <rt/syscall.h>
#include <rt/tick.h>

#include <stdatomic.h>

static RT_LIST(ready_list);

static struct rt_syscall_record *_Atomic pending_syscalls;

static struct rt_task idle_task = {
    .ctx = NULL,
    .list = RT_LIST_INIT(idle_task.list),
    .syscall_record = {.next = NULL, .syscall = RT_SYSCALL_NONE},
    .name = "idle",
    .priority = 0,
};

struct rt_task *rt_prev_task;
static struct rt_task *active_task = &idle_task;

static struct rt_task *task_from_list(struct rt_list *list)
{
    return rt_list_item(list, struct rt_task, list);
}

static struct rt_task *ready_pop(void)
{
    /* TODO: deal with different priorities */
    if (rt_list_is_empty(&ready_list))
    {
        return NULL;
    }
    return task_from_list(rt_list_pop_front(&ready_list));
}

void rt_yield(void)
{
    rt_syscall(RT_SYSCALL_YIELD);
}

static atomic_flag sched_pending = ATOMIC_FLAG_INIT;

void rt_sched(void)
{
    static struct rt_syscall_record sched = {
        .next = NULL,
        .syscall = RT_SYSCALL_SCHED,
    };

    if (!atomic_flag_test_and_set_explicit(&sched_pending,
                                           memory_order_relaxed))
    {
        rt_syscall_push(&sched);
        rt_syscall_post();
    }
}

const char *rt_task_name(void)
{
    return active_task->name;
}

void rt_task_exit(void)
{
    rt_syscall(RT_SYSCALL_EXIT);
}

/* TODO: simplify this based on what syscall actually occurred */
static void *sched(void)
{
    struct rt_task *const next_task = ready_pop();
    if (!next_task)
    {
        return NULL;
    }

    rt_prev_task = active_task;

    /*
     * If the previous task is still ready, re-add it to the ready list.
     */
    if ((rt_prev_task->syscall_record.syscall != RT_SYSCALL_EXIT) &&
        rt_list_is_empty(&rt_prev_task->list))
    {
        rt_list_push_back(&ready_list, &rt_prev_task->list);
    }

    active_task = next_task;

    return next_task->ctx;
}

struct sleep_periodic_args
{
    unsigned long last_wake_tick;
    unsigned long period;
};

void rt_sleep(unsigned long ticks)
{
    rt_syscall_val(RT_SYSCALL_SLEEP, ticks);
}

void rt_sleep_periodic(unsigned long *last_wake_tick, unsigned long period)
{
    struct sleep_periodic_args args = {
        .last_wake_tick = *last_wake_tick,
        .period = period,
    };
    *last_wake_tick += period;
    rt_syscall_ptr(RT_SYSCALL_SLEEP_PERIODIC, &args);
}

/*
 * These globals may only be manipulated in the system call handler.
 */
static RT_LIST(sleep_list);
static unsigned long woken_tick;
static unsigned long next_wake_tick;

static void sleep_until(struct rt_task *task, unsigned long wake_tick)
{
    const unsigned long ticks_until_wake = wake_tick - woken_tick;

    struct rt_list *node;
    rt_list_for_each(node, &sleep_list)
    {
        const struct rt_task *const sleeping_task =
            rt_list_item(node, struct rt_task, list);
        unsigned long sleeping_wake_tick =
            sleeping_task->syscall_record.arg.val;
        if (ticks_until_wake < (sleeping_wake_tick - woken_tick))
        {
            break;
        }
    }

    task->syscall_record.arg.val = wake_tick;
    rt_list_insert_before(&task->list, node);
    if (rt_list_front(&sleep_list) == &task->list)
    {
        next_wake_tick = wake_tick;
    }
}

static struct rt_task *
task_from_syscall_record(struct rt_syscall_record *record)
{
    return rt_container_of(record, struct rt_task, syscall_record);
}

static void sleep_syscall(struct rt_syscall_record *syscall_record)
{
    struct rt_task *task = task_from_syscall_record(syscall_record);
    const unsigned long ticks = task->syscall_record.arg.val;
    /* Only check for 0 ticks in the syscall so that rt_sleep(0) becomes a
     * synonym for rt_yield(). */
    if (ticks > 0)
    {
        sleep_until(task, woken_tick + ticks);
    }
}

static void sleep_periodic_syscall(struct rt_syscall_record *syscall_record)
{
    struct rt_task *task = task_from_syscall_record(syscall_record);
    struct sleep_periodic_args *args = task->syscall_record.arg.ptr;
    const unsigned long last_wake_tick = args->last_wake_tick;
    const unsigned long period = args->period;
    const unsigned long ticks_since_last_wake = woken_tick - last_wake_tick;
    /* If there have been at least as many ticks as the period since the last
     * wake, then the desired wake up tick has already occurred. */
    if (ticks_since_last_wake < period)
    {
        sleep_until(task, last_wake_tick + period);
    }
}

static void tick_syscall(void)
{
    /*
     * The tick counter moves independently of the tick syscall, so process all
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
            struct rt_list *const node = rt_list_front(&sleep_list);
            struct rt_task *const sleeping_task =
                rt_list_item(node, struct rt_task, list);
            const unsigned long sleeping_wake_tick =
                sleeping_task->syscall_record.arg.val;
            if (sleeping_wake_tick != woken_tick)
            {
                /*
                 * Tasks are ordered by when they should wake up, so if we
                 * reach a task that should still be sleeping, we are done
                 * scanning tasks. This task will be the next to wake, unless
                 * another task goes to sleep later, with an earlier wake tick.
                 */
                next_wake_tick = sleeping_wake_tick;
                break;
            }
            rt_list_remove(node);
            rt_list_push_back(&ready_list, node);
        }
    }
}

static atomic_ulong rt_ticks;
static atomic_flag tick_pending = ATOMIC_FLAG_INIT;

void rt_tick_advance(void)
{
    atomic_fetch_add_explicit(&rt_ticks, 1, memory_order_relaxed);

    static struct rt_syscall_record tick_syscall_record = {
        .next = NULL,
        .syscall = RT_SYSCALL_TICK,
    };

    if (!atomic_flag_test_and_set_explicit(&tick_pending, memory_order_relaxed))
    {
        rt_syscall_push(&tick_syscall_record);
        rt_syscall_post();
    }
}

unsigned long rt_tick(void)
{
    return atomic_load_explicit(&rt_ticks, memory_order_relaxed);
}

void rt_syscall(enum rt_syscall syscall)
{
    active_task->syscall_record.syscall = syscall;
    rt_syscall_push(&active_task->syscall_record);
    rt_syscall_post();
}

void rt_syscall_ptr(enum rt_syscall syscall, void *arg)
{
    active_task->syscall_record.arg.ptr = arg;
    rt_syscall(syscall);
}

void rt_syscall_val(enum rt_syscall syscall, unsigned long arg)
{
    active_task->syscall_record.arg.val = arg;
    rt_syscall(syscall);
}

void rt_syscall_push(struct rt_syscall_record *syscall_record)
{
    /* TODO: need to support multiple contexts trying to push the same
     * record. */
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

static void wake_sem_waiters(struct rt_sem *sem)
{
    int waiters = -atomic_load_explicit(&sem->value, memory_order_acquire);
    while ((sem->num_waiters > 0) && (sem->num_waiters > waiters))
    {
        struct rt_list *node = rt_list_pop_front(&sem->wait_list);
        rt_list_push_back(&ready_list, node);
        --sem->num_waiters;
    }
}

static void wake_mutex_waiter(struct rt_mutex *mutex)
{
    if (!rt_list_is_empty(&mutex->wait_list) &&
        !atomic_flag_test_and_set_explicit(&mutex->lock, memory_order_acquire))
    {
        struct rt_list *node = rt_list_pop_front(&mutex->wait_list);
        rt_list_push_back(&ready_list, node);
    }
}

void *rt_syscall_run(void)
{
    /*
     * Take all elements on the pending syscall stack at once. Syscalls added
     * after this step will be on a new stack. This is done to preserve the
     * ordering of syscalls invoked. Otherwise, syscalls that are added to the
     * stack while the stack is being flushed will be handled before tasks that
     * were pushed earlier.
     */
    /* TODO: need to make sure that a syscall post after this swap has occurred
     * results in the syscall handler being called again, otherwise we should
     * just keep handling system calls from the same stack, even if they're out
     * of order. */
    struct rt_syscall_record *syscall_record =
        atomic_exchange_explicit(&pending_syscalls, NULL, memory_order_acquire);
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
        case RT_SYSCALL_YIELD:
            break;
        case RT_SYSCALL_SLEEP:
            sleep_syscall(syscall_record);
            break;
        case RT_SYSCALL_SLEEP_PERIODIC:
            sleep_periodic_syscall(syscall_record);
            break;
        case RT_SYSCALL_EXIT:
            break;
        case RT_SYSCALL_SEM_WAIT:
        {
            struct rt_task *task = task_from_syscall_record(syscall_record);
            struct rt_sem *sem = syscall_record->arg.ptr;
            rt_list_push_back(&sem->wait_list, &task->list);
            ++sem->num_waiters;
            /* Evaluate semaphore wakes here as well in case a post occurred
             * before the wait syscall was handled. */
            wake_sem_waiters(sem);
            break;
        }
        case RT_SYSCALL_MUTEX_LOCK:
        {
            struct rt_task *task = task_from_syscall_record(syscall_record);
            struct rt_mutex *mutex = syscall_record->arg.ptr;
            rt_list_push_back(&mutex->wait_list, &task->list);
            /* Evaluate mutex wakes here as well in case an unlock occurred
             * before the wait syscall was handled. */
            wake_mutex_waiter(mutex);
            break;
        }
        case RT_SYSCALL_SEM_POST:
        {
            struct rt_sem *sem =
                rt_container_of(syscall_record, struct rt_sem,
                                 syscall_record);
            /* Allow another post syscall to occur while wakes are evaluated so
             * that no posts are missed. */
            atomic_flag_clear_explicit(&sem->post_pending,
                                       memory_order_release);
            wake_sem_waiters(sem);
            break;
        }
        case RT_SYSCALL_MUTEX_UNLOCK:
        {
            struct rt_mutex *mutex =
                rt_container_of(syscall_record, struct rt_mutex,
                                 syscall_record);
            /* Allow another unlock syscall to occur while wakes are evaluated
             * so that no unlocks are missed. */
            atomic_flag_clear_explicit(&mutex->unlock_pending,
                                       memory_order_release);
            wake_mutex_waiter(mutex);
            break;
        }
        }
        syscall_record = syscall_record->next;
    }

    return sched();
}

void rt_task_init(struct rt_task *task, void (*fn)(void), void *stack,
                   size_t stack_size, const char *name, unsigned priority)
{
    rt_list_init(&task->list);
    task->syscall_record.next = NULL;
    task->syscall_record.syscall = RT_SYSCALL_NONE;
    task->ctx = rt_context_create(fn, stack, stack_size);
    task->name = name;
    task->priority = priority;
    rt_list_push_back(&ready_list, &task->list);
}
