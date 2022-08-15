#ifndef RT_SEM_H
#define RT_SEM_H

#include <rt/list.h>
#include <rt/rt.h>

#include <limits.h>
#include <stdint.h>

struct rt_sem
{
    struct rt_list wait_list;
    unsigned int value;
    unsigned int max_value;
};

void rt_sem_init(struct rt_sem *sem, unsigned int initial_value);

void rt_sem_binary_init(struct rt_sem *sem, unsigned int initial_value);

void rt_sem_post(struct rt_sem *sem);

void rt_sem_wait(struct rt_sem *sem);

#define RT_SEM(name, initial_value)                                           \
    struct rt_sem name = {                                                    \
        .wait_list = RT_LIST_INIT(name.wait_list),                            \
        .value = initial_value,                                                \
        .max_value = UINT_MAX,                                                 \
    }

#define RT_SEM_BINARY(name, initial_value)                                    \
    struct rt_sem name = {                                                    \
        .wait_list = RT_LIST_INIT(name.wait_list),                            \
        .value = initial_value,                                                \
        .max_value = 1,                                                        \
    }

#endif /* RT_SEM_H */
