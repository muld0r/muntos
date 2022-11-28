#ifndef RT_TASK_H
#define RT_TASK_H

#include <rt/list.h>

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct rt_task;

/*
 * Initialize a task that runs fn(arg) on the given stack, and make it runnable.
 * Must be called before rt_start().
 */
void rt_task_init(struct rt_task *task, void (*fn)(uintptr_t), uintptr_t arg,
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
    struct rt_syscall_record *record;
};

#define RT_TASK(fn, stack, priority)                                           \
    do                                                                         \
    {                                                                          \
        static struct rt_task fn##_task;                                       \
        rt_task_init(&fn##_task, fn, 0, #fn, priority, stack, sizeof(stack));  \
    } while (0)

#define RT_TASK_ARG(fn, arg, stack, priority)                                  \
    do                                                                         \
    {                                                                          \
        static struct rt_task fn##_task;                                       \
        rt_task_init(&fn##_task, fn, arg, #fn "(" #arg ")", priority, stack,   \
                     sizeof(stack));                                           \
    } while (0)

/*
 * Pointer to the previous task's context field, used to store the suspending
 * context during a context switch.
 */
extern void **rt_prev_context;

#endif /* RT_TASK_H */
