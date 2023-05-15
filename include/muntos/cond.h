#ifndef RT_COND_H
#define RT_COND_H

#include <muntos/sem.h>

#include <stdbool.h>

struct rt_cond;
struct rt_mutex;

void rt_cond_init(struct rt_cond *cond);

void rt_cond_signal(struct rt_cond *cond);

void rt_cond_broadcast(struct rt_cond *cond);

void rt_cond_wait(struct rt_cond *cond, struct rt_mutex *mutex);

bool rt_cond_timedwait(struct rt_cond *cond, struct rt_mutex *mutex,
                       unsigned long ticks);

struct rt_cond
{
    struct rt_sem sem;
};

#define RT_COND_INIT(name)                                                     \
    {                                                                          \
        .sem = RT_SEM_INIT_MAX(name.sem, 0, 0),                                \
    }

#define RT_COND(name) struct rt_cond name = RT_COND_INIT(name)

#endif /* RT_COND_H */
