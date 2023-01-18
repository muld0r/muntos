#ifndef RT_ONCE_H
#define RT_ONCE_H

#include <rt/atomic.h>
#include <rt/mutex.h>

#include <stdint.h>

struct rt_once;

/*
 * Run fn exactly once, among all callers using the same rt_once. This function
 * only returns after fn has been called by some task and returned.
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
 * The slow path for using an rt_once, which should not be called directly.
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
