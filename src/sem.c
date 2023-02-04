#include <rt/sem.h>

#include <rt/log.h>
#include <rt/task.h>

static void sem_init_common(struct rt_sem *sem, int initial_value)
{
    rt_list_init(&sem->wait_list);
    sem->post_record.args.sem_post.sem = sem;
    sem->post_record.syscall = RT_SYSCALL_SEM_POST;
    rt_atomic_store_explicit(&sem->value, initial_value, memory_order_relaxed);
    sem->num_waiters = 0;
    rt_atomic_flag_clear_explicit(&sem->post_pending, memory_order_release);
}

void rt_sem_init(struct rt_sem *sem, int initial_value)
{
    sem->max_value = INT_MAX;
    sem_init_common(sem, initial_value);
}

void rt_sem_binary_init(struct rt_sem *sem, int initial_value)
{
    sem->max_value = 1;
    sem_init_common(sem, initial_value);
}

void rt_sem_post(struct rt_sem *sem)
{
    int value = rt_atomic_load_explicit(&sem->value, memory_order_relaxed);
    do
    {
        if (value == sem->max_value)
        {
            /* Semaphore is saturated. */
            return;
        }
    } while (!rt_atomic_compare_exchange_weak_explicit(&sem->value, &value,
                                                       value + 1,
                                                       memory_order_release,
                                                       memory_order_relaxed));

    rt_logf("%s sem post, new value %d\n", rt_task_name(), value + 1);

    /* If the value was less than zero, then there was at least one waiter when
     * we successfully posted. If there isn't already a post system call
     * pending, then create one. */
    /* TODO: if pre-empted after test_and_set but before pushing the syscall,
     * then posts made by higher priority contexts won't pend a syscall when
     * they should. This can be solved by having tasks use their own
     * syscall record when posting. */
    if ((value < 0) &&
        !rt_atomic_flag_test_and_set_explicit(&sem->post_pending,
                                              memory_order_acquire))
    {
        rt_syscall(&sem->post_record);
    }
}

bool rt_sem_trywait(struct rt_sem *sem)
{
    int value = rt_atomic_load_explicit(&sem->value, memory_order_relaxed);
    do
    {
        if (value <= 0)
        {
            return false;
        }
    } while (!rt_atomic_compare_exchange_weak_explicit(&sem->value, &value,
                                                       value - 1,
                                                       memory_order_acquire,
                                                       memory_order_relaxed));
    return true;
}

void rt_sem_wait(struct rt_sem *sem)
{
    const int value =
        rt_atomic_fetch_sub_explicit(&sem->value, 1, memory_order_acquire);

    rt_logf("%s sem wait, new value %d\n", rt_task_name(), value - 1);

    if (value > 0)
    {
        return;
    }

    struct rt_syscall_record wait_record;
    wait_record.args.sem_wait.task = rt_task_self();
    wait_record.args.sem_wait.sem = sem;
    wait_record.syscall = RT_SYSCALL_SEM_WAIT;
    rt_syscall(&wait_record);
}

bool rt_sem_timedwait(struct rt_sem *sem, unsigned long ticks)
{
    const int value =
        rt_atomic_fetch_sub_explicit(&sem->value, 1, memory_order_acquire);

    rt_logf("%s sem timed wait, new value %d\n", rt_task_name(), value - 1);

    if (value > 0)
    {
        return true;
    }

    if (ticks == 0)
    {
        return false;
    }

    struct rt_syscall_record wait_record;
    wait_record.args.sem_timedwait.task = rt_task_self();
    wait_record.args.sem_timedwait.sem = sem;
    wait_record.args.sem_timedwait.ticks = ticks;
    wait_record.syscall = RT_SYSCALL_SEM_TIMEDWAIT;
    rt_task_self()->record = &wait_record;
    rt_syscall(&wait_record);

    return wait_record.syscall != RT_SYSCALL_SEM_TIMEDWAIT;
}
