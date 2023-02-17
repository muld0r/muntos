#ifndef RT_BARRIER_H
#define RT_BARRIER_H

#include <rt/cond.h>
#include <rt/mutex.h>
#include <rt/sem.h>

#include <stdbool.h>

struct rt_barrier;

void rt_barrier_init(struct rt_barrier *barrier, int count);

/*
 * Blocks until count threads have called it, at which point it returns
 * true to one of those threads and false to the others, and resets to its
 * initial state, waiting for another count threads.
 */
bool rt_barrier_wait(struct rt_barrier *barrier);

struct rt_barrier
{
    struct rt_sem sem;
    struct rt_mutex mutex;
    struct rt_cond cond;
    int tasks, count;
};

#define RT_BARRIER_INIT(name, count_)                                          \
    {                                                                          \
        .sem = RT_SEM_INIT(name.sem, count_),                                  \
        .mutex = RT_MUTEX_INIT(name.mutex), .cond = RT_COND_INIT(name.cond),   \
        .tasks = 0, .count = (count_),                                         \
    }

#define RT_BARRIER(name, count)                                                \
    struct rt_barrier name = RT_BARRIER_INIT(name, count)

#endif /* RT_BARRIER_H */
