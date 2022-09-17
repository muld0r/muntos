#ifndef RT_TASK_H
#define RT_TASK_H

#include <rt/list.h>
#include <rt/mutex.h>
#include <rt/sem.h>
#include <rt/syscall.h>

#include <stdbool.h>

struct rt_task;

/*
 * Initialize a task and make it runnable. Must be called before rt_start().
 */
void rt_task_init(struct rt_task *task, void (*fn)(void), void *stack,
                   size_t stack_size, const char *name, unsigned priority);

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
        struct rt_mutex *mutex;
    } syscall_args;
    struct rt_context *ctx;
    const char *name;
    unsigned priority;
};

#endif /* RT_TASK_H */
