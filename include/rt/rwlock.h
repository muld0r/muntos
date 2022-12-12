#ifndef RT_RWLOCK_H
#define RT_RWLOCK_H

#include <rt/cond.h>
#include <rt/mutex.h>

#include <stdbool.h>

struct rt_rwlock;

void rt_rwlock_init(struct rt_rwlock *lock);

void rt_rwlock_rlock(struct rt_rwlock *lock);

void rt_rwlock_runlock(struct rt_rwlock *lock);

void rt_rwlock_wlock(struct rt_rwlock *lock);

void rt_rwlock_wunlock(struct rt_rwlock *lock);

struct rt_rwlock
{
    struct rt_mutex m;
    struct rt_cond rcond, wcond;
    int num_readers, num_writers;
};

#define RT_RWLOCK_INIT(name)                                                   \
    {                                                                          \
        .m = RT_MUTEX_INIT(name.m), .rcond = RT_COND_INIT(name.rcond),         \
        .wcond = RT_COND_INIT(name.wcond), .num_readers = 0, .num_writers = 0, \
    }

#define RT_RWLOCK(name) struct rt_rwlock name = RT_RWLOCK_INIT(name)

#endif /* RT_RWLOCK_H */
