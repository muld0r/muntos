#include <muntos/once.h>

void rt_once_slow(struct rt_once *once, void (*fn)(void))
{
    rt_mutex_lock(&once->mutex);
    /* A mutex has acquire-release semantics, so we can load relaxed here. */
    if (rt_atomic_load_explicit(&once->done, memory_order_relaxed) == 0)
    {
        fn();
        /* This release pairs with the acquire in the fast path. */
        rt_atomic_store_explicit(&once->done, 1, memory_order_release);
    }
    rt_mutex_unlock(&once->mutex);
}
