#include <rt/rt.h>

#include <rt/atomic.h>
#include <rt/container.h>
#include <rt/context.h>
#include <rt/cycle.h>
#include <rt/list.h>
#include <rt/log.h>
#include <rt/mutex.h>
#include <rt/sem.h>
#include <rt/sleep.h>
#include <rt/syscall.h>
#include <rt/task.h>
#include <rt/tick.h>

#define task_from_member(p, m) (rt_container_of((p), struct rt_task, m))
#define task_from_list(l) (task_from_member(l, list))
#define task_from_sleep_list(l) (task_from_member(l, sleep_list))
#define task_from_record(r) (task_from_member(r, record))

static bool task_priority_greater_than(const struct rt_list *a,
                                       const struct rt_list *b)
{
    return task_from_list(a)->priority > task_from_list(b)->priority;
}

static void insert_by_priority(struct rt_list *list, struct rt_task *task)
{
    rt_list_insert_by(list, &task->list, task_priority_greater_than);
}

static RT_LIST(ready_list);

static struct rt_syscall_record *_Atomic pending_syscalls;

static struct rt_task idle_task = {
    .list = RT_LIST_INIT(idle_task.list),
    .sleep_list = RT_LIST_INIT(idle_task.sleep_list),
    .name = "idle",
    .priority = 0,
    /* The idle task is initially running. rt_start() is expected to trigger a
     * switch out of it. */
    .state = RT_TASK_STATE_RUNNING,
};

static struct rt_task *active_task = &idle_task;

void rt_sched(void)
{
    rt_syscall_pend();
}

const char *rt_task_name(void)
{
    return active_task->name;
}

struct rt_task *rt_task_self(void)
{
    return active_task;
}

void rt_task_ready(struct rt_task *task)
{
    rt_logf("syscall: %s ready\n", task->name);
    task->record.syscall = RT_SYSCALL_TASK_READY;
    rt_syscall(&task->record);
}

static void task_ready(struct rt_task *task)
{
    task->state = RT_TASK_STATE_READY;
    insert_by_priority(&ready_list, task);
}

void rt_task_exit(void)
{
    rt_logf("syscall: %s exit\n", rt_task_name());
    active_task->record.syscall = RT_SYSCALL_EXIT;
    rt_syscall(&active_task->record);
}

void **rt_context_prev;

static void *sched(void)
{
    if (rt_list_is_empty(&ready_list))
    {
        rt_logf("sched: no new task to run, continuing %s\n", rt_task_name());
        return NULL;
    }

    struct rt_task *next_task = task_from_list(rt_list_front(&ready_list));

    const bool still_running = active_task->state == RT_TASK_STATE_RUNNING;

    /* If the active task is still running and the new task is lower priority,
     * then continue executing the active task. */
    if (still_running && (active_task->priority > next_task->priority))
    {
        rt_logf("sched: %s is still highest priority (%u > %u)\n",
                rt_task_name(), active_task->priority, next_task->priority);
        return NULL;
    }

    /* The next task will be used, so remove it from the ready list. */
    rt_list_remove(&next_task->list);

    /* If a task made a system call to suspend itself but was then woken up by
     * its own or another system call and is still the highest priority task,
     * it should continue running, so don't context switch. */
    if (active_task == next_task)
    {
        rt_logf("sched: %s was suspended and reawakened\n", rt_task_name());
        return NULL;
    }

    /* If the active task is still running but we are switching to a new task,
     * add the active task to the ready list and mark it as READY. */
    if (still_running)
    {
        rt_logf("sched: %s is still runnable\n", rt_task_name());
        task_ready(active_task);
    }

    rt_context_prev = &active_task->ctx;
    active_task = next_task;
    active_task->state = RT_TASK_STATE_RUNNING;

    rt_logf("sched: switching to %s with priority %u\n", rt_task_name(),
            active_task->priority);

    return active_task->ctx;
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

static void wake_sem_waiters(struct rt_sem *sem)
{
    int waiters = -rt_atomic_load_explicit(&sem->value, memory_order_relaxed);
    if (waiters < 0)
    {
        waiters = 0;
    }
    while (sem->num_waiters > (size_t)waiters)
    {
        struct rt_task *task =
            task_from_list(rt_list_pop_front(&sem->wait_list));
        rt_list_remove(&task->sleep_list);
        task_ready(task);
        --sem->num_waiters;
    }
}

static void tick_syscall(void)
{
    const unsigned long ticks_to_advance = rt_tick() - woken_tick;
    while (!rt_list_is_empty(&sleep_list))
    {
        struct rt_task *const task =
            task_from_sleep_list(rt_list_front(&sleep_list));
        if (ticks_to_advance < (task->wake_tick - woken_tick))
        {
            break;
        }
        /* If the waking task was blocked on a sem_timedwait, remove it
         * from the semaphore's wait list. */
        if (task->record.syscall == RT_SYSCALL_SEM_TIMEDWAIT)
        {
            struct rt_sem *const sem = task->record.args.sem_timedwait.sem;
            rt_atomic_fetch_add_explicit(&sem->value, 1, memory_order_relaxed);
            rt_list_remove(&task->list);
            --sem->num_waiters;
            wake_sem_waiters(sem);
            /* Signal to the task that its sem_timedwait timed out by
             * setting the sem argument to NULL. */
            task->record.args.sem_timedwait.sem = NULL;
        }
        rt_list_remove(&task->sleep_list);
        task_ready(task);
    }
    woken_tick += ticks_to_advance;
}

static rt_atomic_ulong tick;
static rt_atomic_flag tick_pending = RT_ATOMIC_FLAG_INIT;

void rt_tick_advance(void)
{
    const unsigned long old_tick =
        rt_atomic_fetch_add_explicit(&tick, 1, memory_order_relaxed);

    static struct rt_syscall_record tick_record = {
        .next = NULL,
        .syscall = RT_SYSCALL_TICK,
    };

    if (!rt_atomic_flag_test_and_set_explicit(&tick_pending,
                                              memory_order_relaxed))
    {
        rt_logf("syscall: tick %lu\n", old_tick + 1);
        rt_syscall(&tick_record);
    }
}

unsigned long rt_tick(void)
{
    return rt_atomic_load_explicit(&tick, memory_order_relaxed);
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
    rt_syscall_pend();
}

void *rt_syscall_run(void)
{
#if RT_TASK_ENABLE_CYCLES
    static volatile uint64_t total_task_cycles = 0;
    const uint32_t task_cycles = rt_cycle() - active_task->start_cycle;
    active_task->total_cycles += task_cycles;
    total_task_cycles += task_cycles;
#endif

    /*
     * Take all elements on the pending syscall stack at once. Syscalls added
     * after this step will be on a new stack.
     */
    struct rt_syscall_record *record =
        rt_atomic_exchange_explicit(&pending_syscalls, NULL,
                                    memory_order_acquire);
    while (record)
    {
        /* Store the next record in the list now because some syscall records
         * may be re-enabled immediately after they are handled. */
        struct rt_syscall_record *next_record = record->next;
        switch (record->syscall)
        {
        case RT_SYSCALL_TICK:
            rt_atomic_flag_clear_explicit(&tick_pending, memory_order_release);
            tick_syscall();
            break;
        case RT_SYSCALL_SLEEP:
        {
            const unsigned long ticks = record->args.sleep.ticks;
            struct rt_task *const task = task_from_record(record);
            task->state = RT_TASK_STATE_ASLEEP;
            sleep_until(task, woken_tick + ticks);
            break;
        }
        case RT_SYSCALL_SLEEP_PERIODIC:
        {
            const unsigned long last_wake_tick =
                                    record->args.sleep_periodic.last_wake_tick,
                                period = record->args.sleep_periodic.period,
                                ticks_since_last_wake =
                                    woken_tick - last_wake_tick;
            struct rt_task *const task = task_from_record(record);
            /* If there have been at least as many ticks as the period since the
             * last wake, then the desired wake up tick has already occurred. */
            if (ticks_since_last_wake < period)
            {
                task->state = RT_TASK_STATE_ASLEEP;
                sleep_until(task, last_wake_tick + period);
            }
            break;
        }
        case RT_SYSCALL_EXIT:
            task_from_record(record)->state = RT_TASK_STATE_EXITED;
            break;
        case RT_SYSCALL_SEM_WAIT:
        {
            struct rt_sem *const sem = record->args.sem_wait.sem;
            struct rt_task *const task = task_from_record(record);
            task->state = RT_TASK_STATE_BLOCKED;
            insert_by_priority(&sem->wait_list, task);
            ++sem->num_waiters;
            /* Evaluate semaphore wakes here as well in case a post occurred
             * before the wait syscall was handled. */
            wake_sem_waiters(sem);
            break;
        }
        case RT_SYSCALL_SEM_TIMEDWAIT:
        {
            const unsigned long ticks = record->args.sem_timedwait.ticks;
            struct rt_sem *const sem = record->args.sem_timedwait.sem;
            struct rt_task *const task = task_from_record(record);
            task->state = RT_TASK_STATE_BLOCKED_TIMEOUT;
            insert_by_priority(&sem->wait_list, task);
            sleep_until(task, woken_tick + ticks);
            ++sem->num_waiters;
            wake_sem_waiters(sem);
            break;
        }
        case RT_SYSCALL_SEM_POST:
        {
            struct rt_sem *const sem = record->args.sem_post.sem;
            const int increment = record->args.sem_post.increment;
            /* Allow another post syscall from an interrupt to occur while
             * wakes are evaluated so that no posts are missed. */
            if (record == &sem->post_record)
            {
                rt_atomic_flag_clear_explicit(&sem->post_pending,
                                              memory_order_release);
            }
            rt_atomic_fetch_add_explicit(&sem->value, increment,
                                         memory_order_relaxed);
            wake_sem_waiters(sem);
            break;
        }
        case RT_SYSCALL_TASK_READY:
            task_ready(task_from_record(record));
            break;
        }
        record = next_record;
    }

    void *const new_ctx = sched();
#if RT_TASK_ENABLE_CYCLES
    active_task->start_cycle = rt_cycle();
#endif
    return new_ctx;
}

static void task_init(struct rt_task *task, const char *name, unsigned priority)
{
    rt_logf("%s created\n", name);
    task->priority = priority;
    task->wake_tick = 0;
    task->name = name;
    rt_list_init(&task->sleep_list);
    rt_task_ready(task);
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
