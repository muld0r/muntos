#ifndef RT_TASK_H
#define RT_TASK_H

#include <rt/list.h>
#include <rt/mutex.h>
#include <rt/sem.h>
#include <rt/syscall.h>

#include <stdbool.h>
#include <stddef.h>

struct rt_task;

/*
 * Initialize a task that runs fn(arg) on the given stack, and make it runnable.
 * Must be called before rt_start().
 */
void rt_task_init(struct rt_task *task, void (*fn)(void *), void *arg,
                   const char *name, unsigned priority, void *stack,
                   size_t stack_size);

/*
 * Exit from the current task. This should be called automatically when a
 * task function returns.
 */
void rt_task_exit(void);

/*
 * Get the name of the current task.
 */
const char *rt_task_name(void);

struct rt_task
{
    void *ctx;
    struct rt_list list;
    unsigned long wake_tick;
    const char *name;
    unsigned priority;
};

#define RT_TASK(fn, arg, stack, priority)                                     \
    do                                                                         \
    {                                                                          \
        static struct rt_task fn##_task;                                      \
        rt_task_init(&fn##_task, fn, arg,                                     \
                      ((arg != NULL) ? #fn "(" #arg ")" : #fn "()"), priority, \
                      stack, sizeof(stack));                                   \
    } while (0)

/*
 * Pointer to the previous task, used to store the suspending context in a
 * context switch.
 */
extern struct rt_task *rt_prev_task;

#endif /* RT_TASK_H */
