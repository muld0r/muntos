#ifndef RT_TASK_H
#define RT_TASK_H

#include <rt/list.h>
#include <rt/sem.h>
#include <rt/syscall.h>

#include <stdbool.h>

struct rt_task
{
    struct rt_list list;
    struct rt_syscall_record syscall_record;
    union
    {
        unsigned long wake_tick;
        unsigned long sleep_ticks;
        struct
        {
            unsigned long last_wake_tick;
            unsigned long period;
        } sleep_periodic;
        struct rt_sem *sem;
    } syscall_args;
    struct rt_context *ctx;
    void (*fn)(void);
    void *stack;
    size_t stack_size;
    const char *name;
    unsigned priority;
};

#define RT_TASK(name_, fn_, stack_, priority_)                                \
    struct rt_task name_ = {                                                  \
        .list = RT_LIST_INIT(name_.list),                                     \
        .syscall_record = {.next = NULL, .syscall = RT_SYSCALL_NONE},         \
        .ctx = NULL,                                                           \
        .fn = fn_,                                                             \
        .stack = stack_,                                                       \
        .stack_size = sizeof stack_,                                           \
        .name = #name_,                                                        \
        .priority = priority_,                                                 \
    }

/*
 * Launch a newly created task.
 */
void rt_task_start(struct rt_task *task);

/*
 * Make a task runnable.
 */
void rt_task_ready(struct rt_task *task);

#endif /* RT_TASK_H */
