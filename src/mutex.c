#include <rt/mutex.h>

#include <rt/log.h>
#include <rt/task.h>

void rt_mutex_init(struct rt_mutex *mutex)
{
    rt_list_init(&mutex->wait_list);
    mutex->syscall_record.syscall = RT_SYSCALL_MUTEX_UNLOCK;
    mutex->syscall_record.args.mutex = mutex;
    rt_atomic_flag_clear_explicit(&mutex->unlock_pending, memory_order_relaxed);
    rt_atomic_flag_clear_explicit(&mutex->lock, memory_order_release);
}

void rt_mutex_lock(struct rt_mutex *mutex)
{
    if (rt_mutex_trylock(mutex))
    {
        /* Acquired the lock. */
        rt_logf("%s lock\n", rt_task_name());
        return;
    }

    struct rt_syscall_record syscall_record = {
        .syscall = RT_SYSCALL_MUTEX_LOCK,
        .args.mutex = mutex,
    };
    rt_logf("syscall: %s mutex lock\n", rt_task_name());
    rt_syscall(&syscall_record);
}

bool rt_mutex_trylock(struct rt_mutex *mutex)
{
    return !rt_atomic_flag_test_and_set_explicit(&mutex->lock,
                                                 memory_order_acquire);
}

void rt_mutex_unlock(struct rt_mutex *mutex)
{
    /* TODO: make the lock go to the highest priority waiter, rather than
     * whoever gets to the test_and_set first. */
    rt_atomic_flag_clear_explicit(&mutex->lock, memory_order_release);
    rt_logf("%s unlock\n", rt_task_name());

    /* If there isn't already an unlock system call pending, then create one. */
    if (!rt_atomic_flag_test_and_set_explicit(&mutex->unlock_pending,
                                              memory_order_acquire))
    {
        rt_logf("syscall: %s mutex unlock\n", rt_task_name());
        rt_syscall(&mutex->syscall_record);
    }
}
