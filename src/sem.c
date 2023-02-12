#include <rt/sem.h>

#include <rt/interrupt.h>
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
            rt_logf("%s sem post, saturated %d\n", rt_task_name(), value);
            return;
        }
        if (value < 0)
        {
            /* If the value was negative, then the post needs to happen in a
             * system call because there are waiters. */
            rt_logf("%s sem post, syscall %d + 1\n", rt_task_name(), value);
            rt_sem_post_syscall(sem, 1);
            return;
        }
    } while (!rt_atomic_compare_exchange_weak_explicit(&sem->value, &value,
                                                       value + 1,
                                                       memory_order_release,
                                                       memory_order_relaxed));

    rt_logf("%s sem post, new value %d\n", rt_task_name(), value + 1);
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

    struct rt_syscall_record *const wait_record = &rt_task_self()->record;
    wait_record->args.sem_wait.sem = sem;
    wait_record->syscall = RT_SYSCALL_SEM_WAIT;
    rt_syscall(wait_record);
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

    struct rt_syscall_record *const wait_record = &rt_task_self()->record;
    wait_record->args.sem_timedwait.sem = sem;
    wait_record->args.sem_timedwait.ticks = ticks;
    wait_record->syscall = RT_SYSCALL_SEM_TIMEDWAIT;
    rt_syscall(wait_record);

    return wait_record->args.sem_timedwait.sem != NULL;
}

void rt_sem_post_syscall(struct rt_sem *sem, int increment)
{
    /* In an interrupt, we need to use the post system call record attached to
     * the semaphore rather than allocating one on the stack, because the
     * system call will run after the interrupt has returned. */
    if (rt_interrupt_is_active())
    {
        /* If the semaphore's post record is already pending, don't attempt to
         * use it again. The interrupt that is using it will still cause the
         * post to occur, so no posts are missed in this case. Instead, just
         * add to the semaphore value directly. The system call will run
         * after this increment has taken effect. */
        if (!rt_atomic_flag_test_and_set_explicit(&sem->post_pending,
                                                  memory_order_acquire))
        {
            sem->post_record.args.sem_post.increment = increment;
            rt_syscall(&sem->post_record);
        }
        else
        {
            rt_atomic_fetch_add_explicit(&sem->value, increment,
                                         memory_order_release);
        }
    }
    else
    {
        /* Each task can use its own post system call record because the
         * syscall will run before this call returns. Adding to the semaphore
         * value directly from a task when there are waiters can result in
         * priority inversion if a context switch occurs before wakes are
         * resolved but after the value is incremented, and the semaphore is
         * decremented on the fast path by another task that is lower priority
         * than a previous waiter. */
        struct rt_syscall_record *const post_record = &rt_task_self()->record;
        post_record->args.sem_post.sem = sem;
        post_record->args.sem_post.increment = increment;
        post_record->syscall = RT_SYSCALL_SEM_POST;
        rt_syscall(post_record);
    }
}
