#include <rt/cond.h>

void rt_cond_init(struct rt_cond *cond)
{
    rt_sem_init(&cond->sem, 0);
}

static void cond_post(struct rt_sem *sem, bool broadcast)
{
    int value = rt_atomic_load_explicit(&sem->value, memory_order_relaxed);
    do
    {
        if (value >= 0)
        {
            /* Condition variable has no waiters. */
            return;
        }
        /* Use relaxed ordering always because the waiter is using a mutex. */
    } while (!rt_atomic_compare_exchange_weak_explicit(&sem->value, &value,
                                                       broadcast ? 0
                                                                 : value + 1,
                                                       memory_order_relaxed,
                                                       memory_order_relaxed));

    /* TODO: see sem_post */
    if (!rt_atomic_flag_test_and_set_explicit(&sem->post_pending,
                                              memory_order_acquire))
    {
        rt_syscall(&sem->post_record);
    }
}

void rt_cond_signal(struct rt_cond *cond)
{
    cond_post(&cond->sem, false);
}

void rt_cond_broadcast(struct rt_cond *cond)
{
    cond_post(&cond->sem, true);
}

void rt_cond_wait(struct rt_cond *cond, struct rt_mutex *mutex)
{
    rt_mutex_unlock(mutex);
    rt_sem_wait(&cond->sem);
    rt_mutex_lock(mutex);
}
