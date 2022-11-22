#include <rt/mutex.h>

#include <rt/log.h>
#include <rt/task.h>

void rt_mutex_init(struct rt_mutex *mutex)
{
    rt_list_init(&mutex->wait_list);
    mutex->unlock_record.args.mutex_unlock.mutex = mutex;
    mutex->unlock_record.syscall = RT_SYSCALL_MUTEX_UNLOCK;
    mutex->num_waiters = 0;
    rt_atomic_store_explicit(&mutex->lock, 1, memory_order_release);
}

void rt_mutex_lock(struct rt_mutex *mutex)
{
    const int lock =
        rt_atomic_fetch_sub_explicit(&mutex->lock, 1, memory_order_acquire);

    if (lock < 1)
    {
        struct rt_syscall_record lock_record;
        lock_record.args.mutex_lock.task = rt_task_self();
        lock_record.args.mutex_lock.mutex = mutex;
        lock_record.syscall = RT_SYSCALL_MUTEX_LOCK;
        rt_logf("syscall: %s mutex lock\n", rt_task_name());
        rt_syscall(&lock_record);
    }
    rt_logf("%s mutex locked\n", rt_task_name());
}

bool rt_mutex_trylock(struct rt_mutex *mutex)
{
    int lock = 1;
    if (rt_atomic_compare_exchange_strong_explicit(&mutex->lock, &lock, 0,
                                                   memory_order_acquire,
                                                   memory_order_relaxed))
    {
        rt_logf("%s mutex locked\n", rt_task_name());
        return true;
    }
    else
    {
        rt_logf("%s mutex trylock failed\n", rt_task_name());
        return false;
    }
}

bool rt_mutex_timedlock(struct rt_mutex *mutex, unsigned long ticks)
{
    const int lock =
        rt_atomic_fetch_sub_explicit(&mutex->lock, 1, memory_order_acquire);

    if (lock < 1)
    {
        struct rt_syscall_record lock_record;
        lock_record.args.mutex_timedlock.task = rt_task_self();
        lock_record.args.mutex_timedlock.mutex = mutex;
        lock_record.args.mutex_timedlock.ticks = ticks;
        lock_record.syscall = RT_SYSCALL_MUTEX_TIMEDLOCK;
        rt_task_self()->record = &lock_record;
        rt_logf("syscall: %s mutex timedlock\n", rt_task_name());
        rt_syscall(&lock_record);
        if (lock_record.syscall != RT_SYSCALL_MUTEX_TIMEDLOCK)
        {
            rt_logf("%s mutex timedlock failed\n", rt_task_name());
            return false;
        }
    }
    rt_logf("%s mutex locked\n", rt_task_name());
    return true;
}

void rt_mutex_unlock(struct rt_mutex *mutex)
{
    const int lock =
        rt_atomic_fetch_add_explicit(&mutex->lock, 1, memory_order_release);

    rt_logf("%s mutex unlocked\n", rt_task_name());

    if (lock < 0)
    {
        /*
         * There are waiters, so syscall to wake them.
         * NOTE: a trylock from a higher priority context than any of the
         * waiters can spuriously fail here even though the mutex is logically
         * unlocked at this point. A normal lock will suspend itself and then
         * be resumed by the unlock syscall.
         */
        rt_logf("syscall: %s mutex unlock\n", rt_task_name());
        rt_syscall(&mutex->unlock_record);
    }
}
