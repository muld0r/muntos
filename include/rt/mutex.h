#ifndef RT_MUTEX_H
#define RT_MUTEX_H

#include <rt/sem.h>

struct rt_mutex;

void rt_mutex_init(struct rt_mutex *mutex);

void rt_mutex_lock(struct rt_mutex *mutex);

void rt_mutex_unlock(struct rt_mutex *mutex);

bool rt_mutex_trylock(struct rt_mutex *mutex);

bool rt_mutex_timedlock(struct rt_mutex *mutex, unsigned long ticks);

struct rt_mutex
{
    struct rt_sem sem;
};

#define RT_MUTEX_INIT(name)                                                    \
    {                                                                          \
        .sem = RT_SEM_INIT_BINARY(name.sem, 1),                                \
    }

#define RT_MUTEX(name) struct rt_mutex name = RT_MUTEX_INIT(name)

#endif /* RT_MUTEX_H */
