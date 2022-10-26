#ifndef RT_MUTEX_H
#define RT_MUTEX_H

#include <rt/pq.h>
#include <rt/syscall.h>

#include <stdatomic.h>

struct rt_mutex;

void rt_mutex_init(struct rt_mutex *mutex);

void rt_mutex_lock(struct rt_mutex *mutex);

bool rt_mutex_trylock(struct rt_mutex *mutex);

void rt_mutex_unlock(struct rt_mutex *mutex);

struct rt_mutex
{
    struct rt_pq wait_pq;
    struct rt_syscall_record syscall_record;
    atomic_flag lock;
    atomic_flag unlock_pending;
};

#define RT_MUTEX_INIT(name)                                                    \
    {                                                                          \
        .wait_pq = RT_PQ_INIT(name.wait_pq, rt_task_priority_less_than),       \
        .syscall_record =                                                      \
            {                                                                  \
                .next = NULL,                                                  \
                .syscall = RT_SYSCALL_MUTEX_UNLOCK,                            \
                .args.mutex = &name,                                           \
            },                                                                 \
        .lock = ATOMIC_FLAG_INIT, .unlock_pending = ATOMIC_FLAG_INIT,          \
    }

#define RT_MUTEX(name) struct rt_mutex name = RT_MUTEX_INIT(name)

#endif /* RT_MUTEX_H */
