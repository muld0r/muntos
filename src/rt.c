#include <rt/rt.h>

#include <rt/atomic.h>
#include <rt/container.h>
#include <rt/context.h>
#include <rt/list.h>
#include <rt/log.h>
#include <rt/mutex.h>
#include <rt/sem.h>
#include <rt/sleep.h>
#include <rt/syscall.h>
#include <rt/task.h>
#include <rt/tick.h>

#define task_from_member(l, m) (rt_container_of((l), struct rt_task, m))
#define task_from_list(l) (task_from_member(l, list))
#define task_from_sleep_list(l) (task_from_member(l, sleep_list))

static bool task_priority_less_than(const struct rt_list *a,
                                    const struct rt_list *b)
{
    return task_from_list(a)->priority < task_from_list(b)->priority;
}

static void insert_by_priority(struct rt_list *list, struct rt_list *node)
{
    rt_list_insert_by(list, node, task_priority_less_than);
}

static RT_LIST(ready_list);

static struct rt_syscall_record *_Atomic pending_syscalls;

static struct rt_task idle_task = {
    .ctx = NULL,
    .list = RT_LIST_INIT(idle_task.list),
    .sleep_list = RT_LIST_INIT(idle_task.sleep_list),
    .record = NULL,
    .name = "idle",
    .priority = UINT_MAX,
};

static struct rt_task *active_task = &idle_task;

void rt_sched(void)
{
    rt_syscall_post();
}

const char *rt_task_name(void)
{
    return active_task->name;
}

struct rt_task *rt_task_self(void)
{
    return active_task;
}

void rt_task_exit(void)
{
    rt_logf("syscall: %s exit\n", rt_task_name());
    struct rt_syscall_record exit_record;
    exit_record.args.exit.task = rt_task_self();
    exit_record.syscall = RT_SYSCALL_EXIT;
    rt_syscall(&exit_record);
}

void **rt_prev_context;

static void *sched(void)
{
    if (rt_list_is_empty(&ready_list))
    {
        rt_logf("sched: no new task to run, continuing %s\n", rt_task_name());
        return NULL;
    }

    struct rt_list *const node = rt_list_front(&ready_list);
    struct rt_task *next_task = task_from_list(node);

    const bool active_is_runnable = rt_list_is_empty(&active_task->list) &&
                                    rt_list_is_empty(&active_task->sleep_list);

    /* If the active task is still runnable and the new task is lower priority,
     * then continue executing the active task. */
    if (active_is_runnable && (next_task->priority > active_task->priority))
    {
        rt_logf("sched: %s is still highest priority (%u < %u)\n",
                rt_task_name(), active_task->priority, next_task->priority);
        return NULL;
    }

    /* The next task will be used, so remove it from the ready list. */
    rt_list_remove(node);

    /* If a task made a system call to suspend itself but was then woken up by
     * its own or another system call and is still the highest priority task,
     * it should continue running, so don't context switch. */
    if (active_task == next_task)
    {
        return NULL;
    }

    /* If the active task is not already waiting for some other event, re-add
     * it to the ready list. */
    if (active_is_runnable)
    {
        rt_logf("sched: %s is still runnable\n", rt_task_name());
        insert_by_priority(&ready_list, &active_task->list);
    }

    rt_prev_context = &active_task->ctx;
    active_task = next_task;

    rt_logf("sched: switching to %s with priority %u\n", rt_task_name(),
            active_task->priority);
    return next_task->ctx;
}

/*
 * These globals may only be manipulated in the system call handler.
 */
static unsigned long woken_tick;

static bool wake_tick_less_than(const struct rt_list *a,
                                const struct rt_list *b)
{
    return (task_from_sleep_list(a)->wake_tick - woken_tick) <
           (task_from_sleep_list(b)->wake_tick - woken_tick);
}

static RT_LIST(sleep_list);

static void sleep_until(struct rt_task *task, unsigned long wake_tick)
{
    task->wake_tick = wake_tick;
    rt_list_insert_by(&sleep_list, &task->sleep_list, wake_tick_less_than);
}

static void suspend(struct rt_list *list, struct rt_task *task)
{
    insert_by_priority(list, &task->list);
}

static void suspend_with_timeout(struct rt_list *list, struct rt_task *task,
                                 unsigned long ticks)
{
    insert_by_priority(list, &task->list);
    sleep_until(task, woken_tick + ticks);
}

static void wake_if_sleeping(struct rt_task *task)
{
    if (!rt_list_is_empty(&task->sleep_list))
    {
        rt_list_remove(&task->sleep_list);
    }
}

static void wake_sem_waiters(struct rt_sem *sem)
{
    int waiters = -rt_atomic_load_explicit(&sem->value, memory_order_acquire);
    if (waiters < 0)
    {
        waiters = 0;
    }
    while (sem->num_waiters > (size_t)waiters)
    {
        struct rt_task *task =
            task_from_list(rt_list_pop_front(&sem->wait_list));
        wake_if_sleeping(task);
        insert_by_priority(&ready_list, &task->list);
        --sem->num_waiters;
    }
}

static void wake_mutex_waiter(struct rt_mutex *mutex)
{
    int waiters = -rt_atomic_load_explicit(&mutex->lock, memory_order_acquire);
    if (waiters < 0)
    {
        waiters = 0;
    }
    if (mutex->num_waiters > waiters)
    {
        struct rt_task *task =
            task_from_list(rt_list_pop_front(&mutex->wait_list));
        wake_if_sleeping(task);
        insert_by_priority(&ready_list, &task->list);
        --mutex->num_waiters;
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
        while (!rt_list_is_empty(&sleep_list))
        {
            struct rt_task *const task =
                task_from_sleep_list(rt_list_front(&sleep_list));
            if (task->wake_tick != woken_tick)
            {
                /*
                 * Tasks are ordered by when they should wake up, so if we
                 * reach a task that should still be sleeping, we are done
                 * scanning tasks. This task will be the next to wake, unless
                 * another task goes to sleep later, with an earlier wake tick.
                 */
                break;
            }
            if (task->record)
            {
                /* If the waking task was blocked on a timed system call, remove
                 * it from the corresponding wait list and adjust the waiter
                 * counts. */
                if (task->record->syscall == RT_SYSCALL_SEM_TIMEDWAIT)
                {
                    struct rt_sem *sem = task->record->args.sem_timedwait.sem;
                    rt_atomic_fetch_add_explicit(&sem->value, 1,
                                                 memory_order_relaxed);
                    rt_list_remove(&task->list);
                    --sem->num_waiters;
                    wake_sem_waiters(sem);
                    task->record->syscall = RT_SYSCALL_SLEEP;
                }
                else if (task->record->syscall == RT_SYSCALL_MUTEX_TIMEDLOCK)
                {
                    struct rt_mutex *mutex =
                        task->record->args.mutex_timedlock.mutex;
                    rt_atomic_fetch_add_explicit(&mutex->lock, 1,
                                                 memory_order_relaxed);
                    rt_list_remove(&task->list);
                    --mutex->num_waiters;
                    wake_mutex_waiter(mutex);
                    task->record->syscall = RT_SYSCALL_SLEEP;
                }
            }
            rt_list_remove(&task->sleep_list);
            insert_by_priority(&ready_list, &task->list);
        }
    }
}

static rt_atomic_ulong rt_ticks;
static rt_atomic_flag tick_pending = RT_ATOMIC_FLAG_INIT;

void rt_tick_advance(void)
{
    const unsigned long oldticks =
        rt_atomic_fetch_add_explicit(&rt_ticks, 1, memory_order_relaxed);

    static struct rt_syscall_record tick_record = {
        .next = NULL,
        .syscall = RT_SYSCALL_TICK,
    };

    if (!rt_atomic_flag_test_and_set_explicit(&tick_pending,
                                              memory_order_relaxed))
    {
        rt_logf("syscall: tick %lu\n", oldticks + 1);
        rt_syscall(&tick_record);
    }
}

unsigned long rt_tick(void)
{
    return rt_atomic_load_explicit(&rt_ticks, memory_order_relaxed);
}

void rt_syscall(struct rt_syscall_record *record)
{
    record->next =
        rt_atomic_load_explicit(&pending_syscalls, memory_order_relaxed);
    while (!rt_atomic_compare_exchange_weak_explicit(&pending_syscalls,
                                                     &record->next, record,
                                                     memory_order_release,
                                                     memory_order_relaxed))
    {
    }
    rt_syscall_post();
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
    struct rt_syscall_record *record =
        rt_atomic_exchange_explicit(&pending_syscalls, NULL,
                                    memory_order_acquire);
    while (record)
    {
        switch (record->syscall)
        {
        case RT_SYSCALL_TICK:
            rt_atomic_flag_clear_explicit(&tick_pending, memory_order_release);
            tick_syscall();
            break;
        case RT_SYSCALL_SLEEP:
        {
            const unsigned long ticks = record->args.sleep.ticks;
            /* Only check for 0 ticks in the syscall so that rt_sleep(0) becomes
             * a synonym for rt_sched(). */
            if (ticks > 0)
            {
                sleep_until(record->args.sleep.task, woken_tick + ticks);
            }
            break;
        }
        case RT_SYSCALL_SLEEP_PERIODIC:
        {
            const unsigned long last_wake_tick =
                                    record->args.sleep_periodic.last_wake_tick,
                                period = record->args.sleep_periodic.period,
                                ticks_since_last_wake =
                                    woken_tick - last_wake_tick;
            /* If there have been at least as many ticks as the period since the
             * last wake, then the desired wake up tick has already occurred. */
            if (ticks_since_last_wake < period)
            {
                sleep_until(record->args.sleep_periodic.task,
                            last_wake_tick + period);
            }
            break;
        }
        case RT_SYSCALL_EXIT:
        {
            static RT_LIST(exited_list);
            rt_list_push_back(&exited_list, &record->args.exit.task->list);
            break;
        }
        case RT_SYSCALL_SEM_WAIT:
        {
            struct rt_sem *const sem = record->args.sem_wait.sem;
            suspend(&sem->wait_list, record->args.sem_wait.task);
            ++sem->num_waiters;
            /* Evaluate semaphore wakes here as well in case a post occurred
             * before the wait syscall was handled. */
            wake_sem_waiters(sem);
            break;
        }
        case RT_SYSCALL_SEM_TIMEDWAIT:
        {
            struct rt_sem *const sem = record->args.sem_wait.sem;
            suspend_with_timeout(&sem->wait_list, record->args.sem_wait.task,
                                 record->args.sem_timedwait.ticks);
            ++sem->num_waiters;
            wake_sem_waiters(sem);
            break;
        }
        case RT_SYSCALL_SEM_POST:
        {
            struct rt_sem *const sem = record->args.sem_post.sem;
            /* Allow another post syscall to occur while wakes are evaluated so
             * that no posts are missed. */
            rt_atomic_flag_clear_explicit(&sem->post_pending,
                                          memory_order_release);
            wake_sem_waiters(sem);
            break;
        }
        case RT_SYSCALL_MUTEX_LOCK:
        {
            struct rt_mutex *const mutex = record->args.mutex_lock.mutex;
            suspend(&mutex->wait_list, record->args.mutex_lock.task);
            ++mutex->num_waiters;
            /* Evaluate mutex wakes here as well in case an unlock occurred
             * before the wait syscall was handled. */
            wake_mutex_waiter(mutex);
            break;
        }
        case RT_SYSCALL_MUTEX_TIMEDLOCK:
        {
            struct rt_mutex *const mutex = record->args.mutex_timedlock.mutex;
            suspend_with_timeout(&mutex->wait_list,
                                 record->args.mutex_timedlock.task,
                                 record->args.mutex_timedlock.ticks);
            ++mutex->num_waiters;
            wake_mutex_waiter(mutex);
            break;
        }
        case RT_SYSCALL_MUTEX_UNLOCK:
            wake_mutex_waiter(record->args.mutex_unlock.mutex);
            break;
        }
        record = record->next;
    }

    return sched();
}

void rt_task_start(struct rt_task *task)
{
    insert_by_priority(&ready_list, &task->list);
}

static void task_init(struct rt_task *task, const char *name, unsigned priority)
{
    rt_logf("%s created\n", name);
    task->priority = priority;
    task->wake_tick = 0;
    task->record = NULL;
    task->name = name;
    rt_list_init(&task->sleep_list);
    rt_task_start(task);
}

void rt_task_init(struct rt_task *task, void (*fn)(void), const char *name,
                  unsigned priority, void *stack, size_t stack_size)
{
    task->ctx = rt_context_create(fn, stack, stack_size);
    task_init(task, name, priority);
}

void rt_task_init_arg(struct rt_task *task, void (*fn)(uintptr_t),
                      uintptr_t arg, const char *name, unsigned priority,
                      void *stack, size_t stack_size)
{
    task->ctx = rt_context_create_arg(fn, arg, stack, stack_size);
    task_init(task, name, priority);
}
