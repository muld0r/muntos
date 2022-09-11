#ifndef RT_COND_H
#define RT_COND_H

#include <rt/mutex.h>
#include <rt/sem.h>

struct rt_cond;

void rt_cond_init(struct rt_cond *cond);

void rt_cond_signal(struct rt_cond *cond);

void rt_cond_broadcast(struct rt_cond *cond);

void rt_cond_wait(struct rt_cond *cond, struct rt_mutex *mutex);

struct rt_cond
{
    struct rt_sem sem;
};

#define RT_COND(name)                                                         \
    struct rt_cond name = {                                                   \
        .sem = RT_SEM(name, 0),                                               \
    }

#endif /* RT_COND_H */
