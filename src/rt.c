#include <rt/rt.h>

#include <rt/atomic.h>
#include <rt/container.h>
#include <rt/context.h>
#include <rt/list.h>
#include <rt/log.h>
#include <rt/mutex.h>
#include <rt/queue.h>
#include <rt/sem.h>
#include <rt/sleep.h>
#include <rt/syscall.h>
#include <rt/task.h>
#include <rt/tick.h>

#define task_from_list(list_) (rt_container_of((list_), struct rt_task, list))

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
    .name = "idle",
    .priority = UINT_MAX,
};

static struct rt_task *active_task = &idle_task;

void rt_sched(void)
{
    rt_logf("syscall: %s sched\n", rt_task_name());
    struct rt_syscall_record sched_record;
    sched_record.syscall = RT_SYSCALL_SCHED;
    rt_syscall(&sched_record);
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

    const bool active_is_runnable = rt_list_is_empty(&active_task->list);

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

void rt_sleep(unsigned long ticks)
{
    struct rt_syscall_record sleep_record;
    sleep_record.syscall = RT_SYSCALL_SLEEP;
    sleep_record.args.sleep.task = rt_task_self();
    sleep_record.args.sleep.ticks = ticks;
    rt_logf("syscall: %s sleep %lu\n", rt_task_name(), ticks);
    rt_syscall(&sleep_record);
}

void rt_sleep_periodic(unsigned long *last_wake_tick, unsigned long period)
{
    struct rt_syscall_record sleep_record;
    sleep_record.syscall = RT_SYSCALL_SLEEP_PERIODIC;
    sleep_record.args.sleep_periodic.task = rt_task_self();
    sleep_record.args.sleep_periodic.last_wake_tick = *last_wake_tick;
    sleep_record.args.sleep_periodic.period = period;
    rt_logf("syscall: %s sleep periodic, last wake = %lu, period = %lu\n",
            rt_task_name(), *last_wake_tick, period);
    *last_wake_tick += period;
    rt_syscall(&sleep_record);
}

/*
 * These globals may only be manipulated in the system call handler.
 */
static unsigned long woken_tick;
static unsigned long next_wake_tick;

static bool wake_tick_less_than(const struct rt_list *a,
                                const struct rt_list *b)
{
    return (task_from_list(a)->wake_tick - woken_tick) <
           (task_from_list(b)->wake_tick - woken_tick);
}

static RT_LIST(sleep_list);

static void sleep_until(struct rt_task *task, unsigned long wake_tick)
{
    task->wake_tick = wake_tick;
    rt_list_insert_by(&sleep_list, &task->list, wake_tick_less_than);
    if (rt_list_front(&sleep_list) == &task->list)
    {
        next_wake_tick = wake_tick;
    }
}

static void sleep_syscall(struct rt_syscall_record *record)
{
    const unsigned long ticks = record->args.sleep.ticks;
    /* Only check for 0 ticks in the syscall so that rt_sleep(0) becomes a
     * synonym for rt_yield(). */
    if (ticks > 0)
    {
        sleep_until(record->args.sleep.task, woken_tick + ticks);
    }
}

static void sleep_periodic_syscall(struct rt_syscall_record *record)
{
    const unsigned long last_wake_tick =
        record->args.sleep_periodic.last_wake_tick;
    const unsigned long period = record->args.sleep_periodic.period;
    const unsigned long ticks_since_last_wake = woken_tick - last_wake_tick;
    /* If there have been at least as many ticks as the period since the last
     * wake, then the desired wake up tick has already occurred. */
    if (ticks_since_last_wake < period)
    {
        sleep_until(record->args.sleep_periodic.task, last_wake_tick + period);
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
            const struct rt_task *const task = task_from_list(node);
            if (task->wake_tick != woken_tick)
            {
                /*
                 * Tasks are ordered by when they should wake up, so if we
                 * reach a task that should still be sleeping, we are done
                 * scanning tasks. This task will be the next to wake, unless
                 * another task goes to sleep later, with an earlier wake tick.
                 */
                next_wake_tick = task->wake_tick;
                break;
            }
            rt_list_remove(node);
            insert_by_priority(&ready_list, node);
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

static void wake_sem_waiters(struct rt_sem *sem)
{
    int waiters = -rt_atomic_load_explicit(&sem->value, memory_order_acquire);
    if (waiters < 0)
    {
        waiters = 0;
    }
    while (sem->num_waiters > (size_t)waiters)
    {
        insert_by_priority(&ready_list, rt_list_pop_front(&sem->wait_list));
        --sem->num_waiters;
    }
}

static void wake_mutex_waiter(struct rt_mutex *mutex)
{
    int waiters = -rt_atomic_load_explicit(&mutex->lock, memory_order_acquire);
    if (mutex->num_waiters > waiters)
    {
        insert_by_priority(&ready_list, rt_list_pop_front(&mutex->wait_list));
        --mutex->num_waiters;
    }
}

static void wake_queue_waiters(struct rt_queue *queue)
{
    long level = rt_atomic_load_explicit(&queue->level, memory_order_relaxed);

    size_t waiters = 0;
    if (level < 0)
    {
        waiters = (size_t)-level;
    }
    while (queue->num_senders > waiters)
    {
        insert_by_priority(&ready_list, rt_list_pop_front(&queue->send_list));
        --queue->num_senders;
    }

    waiters = 0;
    if (level > (long)queue->num_elems)
    {
        waiters = (size_t)level - queue->num_elems;
    }
    while (queue->num_recvers > waiters)
    {
        insert_by_priority(&ready_list, rt_list_pop_front(&queue->recv_list));
        --queue->num_recvers;
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
    struct rt_syscall_record *record =
        rt_atomic_exchange_explicit(&pending_syscalls, NULL,
                                    memory_order_acquire);
    while (record)
    {
        switch (record->syscall)
        {
        case RT_SYSCALL_SCHED:
            break;
        case RT_SYSCALL_TICK:
            rt_atomic_flag_clear_explicit(&tick_pending, memory_order_release);
            tick_syscall();
            break;
        case RT_SYSCALL_SLEEP:
            sleep_syscall(record);
            break;
        case RT_SYSCALL_SLEEP_PERIODIC:
            sleep_periodic_syscall(record);
            break;
        case RT_SYSCALL_EXIT:
        {
            static RT_LIST(exited_list);
            rt_list_push_back(&exited_list, &record->args.exit.task->list);
            break;
        }
        case RT_SYSCALL_SEM_WAIT:
        {
            struct rt_sem *const sem = record->args.sem_wait.sem;
            insert_by_priority(&sem->wait_list,
                               &record->args.sem_wait.task->list);
            ++sem->num_waiters;
            /* Evaluate semaphore wakes here as well in case a post occurred
             * before the wait syscall was handled. */
            wake_sem_waiters(sem);
            break;
        }
        case RT_SYSCALL_MUTEX_LOCK:
        {
            struct rt_mutex *const mutex = record->args.mutex_lock.mutex;
            insert_by_priority(&mutex->wait_list,
                               &record->args.mutex_lock.task->list);
            ++mutex->num_waiters;
            /* Evaluate mutex wakes here as well in case an unlock occurred
             * before the wait syscall was handled. */
            wake_mutex_waiter(mutex);
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
        case RT_SYSCALL_MUTEX_UNLOCK:
        {
            wake_mutex_waiter(record->args.mutex_unlock.mutex);
            break;
        }
        case RT_SYSCALL_QUEUE_SEND:
        {
            struct rt_queue *const queue = record->args.queue_send.queue;
            insert_by_priority(&queue->send_list,
                               &record->args.queue_send.task->list);
            ++queue->num_senders;
            wake_queue_waiters(queue);
            break;
        }
        case RT_SYSCALL_QUEUE_RECV:
        {
            struct rt_queue *const queue = record->args.queue_recv.queue;
            insert_by_priority(&queue->recv_list,
                               &record->args.queue_recv.task->list);
            ++queue->num_recvers;
            wake_queue_waiters(queue);
            break;
        }
        case RT_SYSCALL_QUEUE_WAKE:
        {
            struct rt_queue *const queue = record->args.queue_wake.queue;
            /* Allow another queue wake syscall to occur while wakes are
             * evaluated so that no wakes are missed. */
            rt_atomic_flag_clear_explicit(&queue->wake_pending,
                                          memory_order_release);
            wake_queue_waiters(queue);
            break;
        }
        }
        record = record->next;
    }

    return sched();
}

void rt_task_init(struct rt_task *task, void (*fn)(void *), void *arg,
                  const char *name, unsigned priority, void *stack,
                  size_t stack_size)
{
    rt_logf("%s created\n", name);
    task->ctx = rt_context_create(fn, arg, stack, stack_size);
    task->priority = priority;
    task->wake_tick = 0;
    task->name = name;
    insert_by_priority(&ready_list, &task->list);
}
