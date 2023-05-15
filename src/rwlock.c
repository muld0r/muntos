#include <muntos/rwlock.h>

void rt_rwlock_init(struct rt_rwlock *lock)
{
    rt_mutex_init(&lock->m);
    rt_cond_init(&lock->rcond);
    rt_cond_init(&lock->wcond);
    lock->num_readers = 0;
    lock->num_writers = 0;
}

void rt_rwlock_rlock(struct rt_rwlock *lock)
{
    rt_mutex_lock(&lock->m);
    while (lock->num_writers > 0)
    {
        rt_cond_wait(&lock->rcond, &lock->m);
    }
    ++lock->num_readers;
    rt_mutex_unlock(&lock->m);
}

void rt_rwlock_runlock(struct rt_rwlock *lock)
{
    rt_mutex_lock(&lock->m);
    --lock->num_readers;
    if (lock->num_readers == 0)
    {
        rt_cond_broadcast(&lock->wcond);
    }
    rt_mutex_unlock(&lock->m);
}

void rt_rwlock_wlock(struct rt_rwlock *lock)
{
    rt_mutex_lock(&lock->m);
    ++lock->num_writers;
    while (lock->num_readers > 0)
    {
        rt_cond_wait(&lock->wcond, &lock->m);
    }
}

void rt_rwlock_wunlock(struct rt_rwlock *lock)
{
    --lock->num_writers;
    if (lock->num_writers == 0)
    {
        rt_cond_broadcast(&lock->rcond);
    }
    rt_mutex_unlock(&lock->m);
}
