#ifndef RT_TASK_H
#define RT_TASK_H

#include <rt/list.h>

#include <limits.h>
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

/*
 * Get the current task.
 */
struct rt_task *rt_task_self(void);

struct rt_task
{
    struct rt_list list;
    struct rt_list sleep_list;
    void *ctx;
    unsigned long wake_tick;
    const char *name;
    unsigned priority;
    int syscall_result;
};

#define RT_TASK(fn, arg, stack, priority)                                      \
    do                                                                         \
    {                                                                          \
        static struct rt_task fn##_task;                                       \
        rt_task_init(&fn##_task, fn, arg,                                      \
                     ((arg != NULL) ? #fn "(" #arg ")" : #fn), priority,       \
                     stack, sizeof(stack));                                    \
    } while (0)

/*
 * Pointer to the previous task's context field, used to store the suspending
 * context during a context switch.
 */
extern void **rt_prev_context;

#endif /* RT_TASK_H */
