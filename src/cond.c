#include <rt/cond.h>

#include <rt/log.h>
#include <rt/mutex.h>
#include <rt/task.h>
#include <rt/tick.h>

void rt_cond_init(struct rt_cond *cond)
{
    rt_sem_init_max(&cond->sem, 0, 0);
}

void rt_cond_signal(struct rt_cond *cond)
{
    rt_logf("%s cond signal\n", rt_task_name());
    rt_sem_post(&cond->sem);
}

void rt_cond_broadcast(struct rt_cond *cond)
{
    rt_logf("%s cond broadcast\n", rt_task_name());
    rt_sem_post_n(&cond->sem, INT_MAX);
}

void rt_cond_wait(struct rt_cond *cond, struct rt_mutex *mutex)
{
    /* Decrement the semaphore while still holding the mutex so that
     * signals from higher priority tasks on the same monitor can see
     * there is a waiter. */
    const int value =
        rt_atomic_fetch_sub_explicit(&cond->sem.value, 1, memory_order_relaxed);

    rt_logf("%s cond wait, new value %d\n", rt_task_name(), value - 1);

    rt_mutex_unlock(mutex);

    rt_logf("%s cond wait, waiting\n", rt_task_name());

    struct rt_syscall_record *const wait_record = &rt_task_self()->record;
    wait_record->args.sem_wait.sem = &cond->sem;
    wait_record->syscall = RT_SYSCALL_SEM_WAIT;
    rt_syscall(wait_record);

    rt_logf("%s cond wait, awoken\n", rt_task_name());

    rt_mutex_lock(mutex);
}

bool rt_cond_timedwait(struct rt_cond *cond, struct rt_mutex *mutex,
                       unsigned long ticks)
{
    const unsigned long start_tick = rt_tick();
    const int value =
        rt_atomic_fetch_sub_explicit(&cond->sem.value, 1, memory_order_relaxed);

    rt_logf("%s cond wait, new value %d\n", rt_task_name(), value - 1);

    rt_mutex_unlock(mutex);

    if (ticks == 0)
    {
        return false;
    }

    struct rt_syscall_record *const record = &rt_task_self()->record;
    record->args.sem_timedwait.sem = &cond->sem;
    record->args.sem_timedwait.ticks = ticks;
    record->syscall = RT_SYSCALL_SEM_TIMEDWAIT;
    rt_syscall(record);

    if (record->args.sem_timedwait.sem == NULL)
    {
        return false;
    }

    const unsigned long ticks_waited = rt_tick() - start_tick;
    if (ticks_waited >= ticks)
    {
        ticks = 0;
    }
    else
    {
        ticks -= ticks_waited;
    }

    return rt_mutex_timedlock(mutex, ticks);
}
