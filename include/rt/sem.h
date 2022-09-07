#ifndef RT_SEM_H
#define RT_SEM_H

#include <rt/list.h>
#include <rt/syscall.h>
#include <rt/rt.h>

#include <limits.h>
#include <stdatomic.h>
#include <stdint.h>

struct rt_sem
{
    struct rt_list wait_list;
    struct rt_syscall_record syscall_record;
    struct atomic_flag post_pending;
    int num_waiters;
    atomic_int value;
    int max_value;
};

void rt_sem_init(struct rt_sem *sem, int initial_value);

void rt_sem_binary_init(struct rt_sem *sem, int initial_value);

void rt_sem_post(struct rt_sem *sem);

void rt_sem_wait(struct rt_sem *sem);

#define RT_SEM_WITH_MAX(name, initial_value, max_value_)                      \
    struct rt_sem name = {                                                    \
        .wait_list = RT_LIST_INIT(name.wait_list),                            \
        .syscall_record = {.next = NULL, .syscall = RT_SYSCALL_NONE},         \
        .post_pending = ATOMIC_FLAG_INIT,                                      \
        .num_waiters = 0,                                                      \
        .value = initial_value,                                                \
        .max_value = max_value_,                                               \
    }

#define RT_SEM(name, initial_value)                                           \
    RT_SEM_WITH_MAX(name, initial_value, INT_MAX)

#define RT_SEM_BINARY(name, initial_value)                                    \
    RT_SEM_WITH_MAX(name, initial_value, 1)

#endif /* RT_SEM_H */
