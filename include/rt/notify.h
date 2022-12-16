#ifndef RT_NOTIFY_H
#define RT_NOTIFY_H

#include <rt/atomic.h>
#include <rt/sem.h>

#include <stdbool.h>
#include <string.h>

struct rt_notify
{
    struct rt_sem sem;
    rt_atomic_uint32_t value;
};

void rt_notify_init(struct rt_notify *note, uint32_t value);

#define RT_NOTIFY(name, value_)                                                \
    struct rt_notify name = {                                                  \
        .sem = RT_SEM_INIT_BINARY(name.sem, 0),                                \
        .value = (value_),                                                     \
    }

/*
 * Notify without changing the notification value.
 */
void rt_notify(struct rt_notify *note);

/*
 * Notify and |= the notification value.
 */
void rt_notify_or(struct rt_notify *note, uint32_t value);

/*
 * Notify and += the notification value.
 */
void rt_notify_add(struct rt_notify *note, uint32_t value);

/*
 * Notify set the notification value unconditionally.
 */
void rt_notify_set(struct rt_notify *note, uint32_t value);

/*
 * Block until notified, and return the notification value. Only one task may
 * call rt_notify_*wait* for each struct rt_notify.
 */
uint32_t rt_notify_wait(struct rt_notify *note);

/*
 * Block the current task until notified, and &= ~clear the notification value.
 * The returned value is before clearing.
 */
uint32_t rt_notify_wait_clear(struct rt_notify *note, uint32_t clear);

/*
 * Get the notification value if one is pending. Returns false if there is no
 * notification pending.
 */
bool rt_notify_trywait(struct rt_notify *note, uint32_t *value);

/*
 * rt_notify_trywait and &= ~clear the notification value after fetching it.
 */
bool rt_notify_trywait_clear(struct rt_notify *note, uint32_t clear,
                             uint32_t *value);

/*
 * Wait for a pending notification until a timeout expires and get the value if
 * a notification occurs. Returns false if there is no notification before the
 * timeout expires.
 */
bool rt_notify_timedwait(struct rt_notify *note, uint32_t *value,
                         unsigned long ticks);

/*
 * rt_notify_timedwait and &= ~clear the notification value after fetching it.
 */
bool rt_notify_timedwait_clear(struct rt_notify *note, uint32_t clear,
                               uint32_t *value, unsigned long ticks);

#endif /* RT_NOTIFY_H */
