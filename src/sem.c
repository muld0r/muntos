#include <rt/sem.h>

#include <rt/rt.h>

static void sem_init_common(struct rt_sem *sem, int initial_value)
{
    rt_list_init(&sem->wait_list);
    sem->syscall_record.next = NULL;
    sem->syscall_record.syscall = RT_SYSCALL_SEM_POST;
    sem->num_waiters = 0;
    atomic_store_explicit(&sem->value, initial_value, memory_order_relaxed);
    atomic_flag_clear_explicit(&sem->post_pending, memory_order_release);
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
    int value = atomic_load_explicit(&sem->value, memory_order_relaxed);
    do
    {
        if (value == sem->max_value)
        {
            /* Semaphore is saturated. */
            return;
        }
    } while (!atomic_compare_exchange_weak_explicit(&sem->value, &value,
                                                    value + 1,
                                                    memory_order_release,
                                                    memory_order_relaxed));

    /* If the value was less than zero, then there was at least one waiter when
     * we successfully posted. If there isn't already a post system call
     * pending, then create one. */
    /* TODO: if pre-empted after test_and_set but before pushing the syscall,
     * then posts made by higher priority contexts won't pend a syscall when
     * they should. This can be solved by having tasks use their own
     * syscall_record when posting. */
    if ((value < 0) && !atomic_flag_test_and_set_explicit(&sem->post_pending,
                                                          memory_order_acquire))
    {
        rt_syscall_push(&sem->syscall_record);
        rt_syscall_post();
    }
}

bool rt_sem_trywait(struct rt_sem *sem)
{
    int value = atomic_load_explicit(&sem->value, memory_order_relaxed);
    do
    {
        if (value <= 0)
        {
            return false;
        }
    } while (!atomic_compare_exchange_weak_explicit(&sem->value, &value,
                                                    value - 1,
                                                    memory_order_acquire,
                                                    memory_order_relaxed));
    return true;
}

void rt_sem_wait(struct rt_sem *sem)
{
    int value = atomic_load_explicit(&sem->value, memory_order_relaxed);
    while (!atomic_compare_exchange_weak_explicit(&sem->value, &value,
                                                  value - 1,
                                                  memory_order_acquire,
                                                  memory_order_relaxed))
    {
    }

    if (value <= 0)
    {
        struct rt_task *self = rt_self();
        self->syscall_args.sem = sem;
        self->syscall_record.syscall = RT_SYSCALL_SEM_WAIT;
        rt_syscall_push(&self->syscall_record);
        rt_syscall_post();
    }
}
