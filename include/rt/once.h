#ifndef RT_ONCE_H
#define RT_ONCE_H

#include <rt/atomic.h>
#include <rt/mutex.h>

struct rt_once;

/*
 * Run fn exactly once among all callers using the same struct rt_once.
 * Regardless of which caller actually executes fn, rt_once only returns for
 * any caller after fn has returned.
 */
static inline void rt_once(struct rt_once *once, void (*fn)(void));

struct rt_once
{
    rt_atomic_int done;
    struct rt_mutex mutex;
};

#define RT_ONCE_INIT(name)                                                     \
    {                                                                          \
        .done = 0, .mutex = RT_MUTEX_INIT(name.mutex),                         \
    }

#define RT_ONCE(name) struct rt_once name = RT_ONCE_INIT(name)

/*
 * The slow path for rt_once. Do not call this directly.
 */
void rt_once_slow(struct rt_once *once, void (*fn)(void));

static inline void rt_once(struct rt_once *once, void (*fn)(void))
{
    if (rt_atomic_load_explicit(&once->done, memory_order_acquire) == 0)
    {
        rt_once_slow(once, fn);
    }
}

#endif /* RT_ONCE_H */
