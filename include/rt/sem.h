#ifndef RT_SEM_H
#define RT_SEM_H

#include <rt/atomic.h>
#include <rt/list.h>
#include <rt/syscall.h>

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>

struct rt_sem;

void rt_sem_init(struct rt_sem *sem, int count);

void rt_sem_init_binary(struct rt_sem *sem, int count);

void rt_sem_init_max(struct rt_sem *sem, int count, int max);

void rt_sem_post(struct rt_sem *sem);

void rt_sem_post_n(struct rt_sem *sem, int n);

void rt_sem_wait(struct rt_sem *sem);

bool rt_sem_trywait(struct rt_sem *sem);

bool rt_sem_timedwait(struct rt_sem *sem, unsigned long ticks);

void rt_sem_add_n(struct rt_sem *sem, int n);

struct rt_sem
{
    struct rt_list wait_list;
    struct rt_syscall_record post_record;
    rt_atomic_int value;
    int max_value;
    size_t num_waiters;
    rt_atomic_flag post_pending;
};

#define RT_SEM_INIT_MAX(name, count, max)                                      \
    {                                                                          \
        .wait_list = RT_LIST_INIT(name.wait_list),                             \
        .post_record =                                                         \
            {                                                                  \
                .next = NULL,                                                  \
                .args.sem_post.sem = &name,                                    \
                .syscall = RT_SYSCALL_SEM_POST,                                \
            },                                                                 \
        .value = count, .max_value = max, .num_waiters = 0,                    \
        .post_pending = RT_ATOMIC_FLAG_INIT,                                   \
    }

#define RT_SEM_INIT(name, count) RT_SEM_INIT_MAX(name, count, INT_MAX)

#define RT_SEM_INIT_BINARY(name, count) RT_SEM_INIT_MAX(name, count, 1)

#define RT_SEM_MAX(name, count, max)                                           \
    struct rt_sem name = RT_SEM_INIT_MAX(name, count, max)

#define RT_SEM(name, count) RT_SEM_MAX(name, count, INT_MAX)

#define RT_SEM_BINARY(name, count) RT_SEM_MAX(name, count, 1)

#endif /* RT_SEM_H */
