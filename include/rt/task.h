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

/*
 * Cause the current task to exit. This should be called automatically when a
 * task function returns.
 */
void rt_task_exit(void);

struct rt_task
{
    void *ctx;
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
    const char *name;
    unsigned priority;
};

#define RT_TASK(fn, stack, priority)                                          \
    do                                                                         \
    {                                                                          \
        static struct rt_task fn##task;                                       \
        rt_task_init(&fn##task, fn, stack, sizeof(stack), #fn, priority);     \
    } while (0)

/*
 * Pointer to the previous task, used to store the suspending context in a
 * context switch.
 */
extern struct rt_task *rt_prev_task;

#endif /* RT_TASK_H */
