#include <rt/notify.h>

void rt_notify(struct rt_notify *note)
{
    rt_sem_post(&note->sem);
}

void rt_notify_or(struct rt_notify *note, uint32_t value)
{
    rt_atomic_fetch_or_explicit(&note->value, value, memory_order_relaxed);
    rt_notify(note);
}

void rt_notify_add(struct rt_notify *note, uint32_t value)
{
    rt_atomic_fetch_add_explicit(&note->value, value, memory_order_relaxed);
    rt_notify(note);
}

void rt_notify_set(struct rt_notify *note, uint32_t value)
{
    rt_atomic_store_explicit(&note->value, value, memory_order_relaxed);
    rt_notify(note);
}

uint32_t rt_notify_wait(struct rt_notify *note)
{
    rt_sem_wait(&note->sem);
    return rt_atomic_load_explicit(&note->value, memory_order_relaxed);
}

uint32_t rt_notify_wait_clear(struct rt_notify *note, uint32_t clear)
{
    rt_sem_wait(&note->sem);
    return rt_atomic_fetch_and_explicit(&note->value, ~clear,
                                        memory_order_relaxed);
}

bool rt_notify_trywait(struct rt_notify *note, uint32_t *value)
{
    if (!rt_sem_trywait(&note->sem))
    {
        return false;
    }
    *value = rt_atomic_load_explicit(&note->value, memory_order_relaxed);
    return true;
}

bool rt_notify_trywait_clear(struct rt_notify *note, uint32_t clear,
                             uint32_t *value)
{
    if (!rt_sem_trywait(&note->sem))
    {
        return false;
    }
    *value = rt_atomic_fetch_and_explicit(&note->value, ~clear,
                                          memory_order_relaxed);
    return true;
}

bool rt_notify_timedwait(struct rt_notify *note, uint32_t *value,
                         unsigned long ticks)
{
    if (!rt_sem_timedwait(&note->sem, ticks))
    {
        return false;
    }
    *value = rt_atomic_load_explicit(&note->value, memory_order_relaxed);
    return true;
}

bool rt_notify_timedwait_clear(struct rt_notify *note, uint32_t clear,
                               uint32_t *value, unsigned long ticks)
{
    if (!rt_sem_timedwait(&note->sem, ticks))
    {
        return false;
    }
    *value = rt_atomic_fetch_and_explicit(&note->value, ~clear,
                                          memory_order_relaxed);
    return true;
}
