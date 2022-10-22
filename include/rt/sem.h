#ifndef RT_SEM_H
#define RT_SEM_H

#include <rt/sbheap.h>
#include <rt/syscall.h>

#include <limits.h>
#include <stdatomic.h>
#include <stdint.h>

struct rt_sem;

void rt_sem_init(struct rt_sem *sem, int initial_value);

void rt_sem_binary_init(struct rt_sem *sem, int initial_value);

void rt_sem_post(struct rt_sem *sem);

void rt_sem_post_all(struct rt_sem *sem);

void rt_sem_wait(struct rt_sem *sem);

bool rt_sem_trywait(struct rt_sem *sem);

struct rt_sem
{
    struct rt_sbheap wait_heap;
    struct rt_syscall_record syscall_record;
    atomic_int value;
    int max_value;
    atomic_flag post_pending;
};

#define RT_SEM_INIT_WITH_MAX(name, initial_value, max_value_)                  \
    {                                                                          \
        .wait_heap =                                                           \
            RT_SBHEAP_INIT(name.wait_heap, rt_task_priority_less_than),        \
        .syscall_record =                                                      \
            {                                                                  \
                .next = NULL,                                                  \
                .syscall = RT_SYSCALL_SEM_POST,                                \
                .args.sem = &name,                                             \
            },                                                                 \
        .post_pending = ATOMIC_FLAG_INIT, .value = initial_value,              \
        .max_value = max_value_,                                               \
    }

#define RT_SEM_INIT(name, initial_value)                                       \
    RT_SEM_INIT_WITH_MAX(name, initial_value, INT_MAX)

#define RT_SEM_BINARY_INIT(name, initial_value)                                \
    RT_SEM_INIT_WITH_MAX(name, initial_value, 1)

#define RT_SEM_WITH_MAX(name, initial_value, max_value_)                       \
    struct rt_sem name = RT_SEM_INIT_WITH_MAX(name, initial_value, max_value_)

#define RT_SEM(name, initial_value)                                            \
    RT_SEM_WITH_MAX(name, initial_value, INT_MAX)

#define RT_SEM_BINARY(name, initial_value)                                     \
    RT_SEM_WITH_MAX(name, initial_value, 1)

#endif /* RT_SEM_H */
