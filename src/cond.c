#include <rt/cond.h>

void rt_cond_init(struct rt_cond *cond)
{
    rt_sem_init(&cond->sem, 0);
}

void rt_cond_signal(struct rt_cond *cond)
{
    rt_sem_post(&cond->sem);
}

void rt_cond_broadcast(struct rt_cond *cond)
{
    rt_sem_post_all(&cond->sem);
}

void rt_cond_wait(struct rt_cond *cond, struct rt_mutex *mutex)
{
    rt_mutex_unlock(mutex);
    rt_sem_wait(&cond->sem);
    rt_mutex_lock(mutex);
}
