#pragma once

#include <rt/context.h>
#include <rt/tick.h>

#include <rt/list.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct rt_task_config
{
    void (*fn)(void);
    void *stack;
    size_t stack_size;
    const char *name;
    unsigned priority;
};

struct rt_task
{
    struct list list;
    struct list event_list;
    struct rt_task_config cfg;
    struct rt_context *ctx;
    rt_tick_t wake_tick;
};

/*
 * Initialize a task.
 */
void rt_task_init(struct rt_task *task, const struct rt_task_config *cfg);

/*
 * Start the rt scheduler.
 */
void rt_start(void);

/*
 * Stop the rt scheduler.
 */
void rt_stop(void);

/*
 * Yield control of the processor to another runnable task.
 */
void rt_yield(void);

/*
 * Select the next task to run and make it active. Used by arch-specific
 * implementations after saving the active context and before loading the next
 * context.
 */
void rt_sched(void);

/*
 * Suspend a task.
 */
void rt_task_suspend(struct rt_task *task);

/*
 * Resume a task.
 */
void rt_task_resume(struct rt_task *task);

/*
 * End a task.
 */
void rt_task_exit(struct rt_task *task);

/*
 * Get a pointer to the currently executing task.
 */
struct rt_task *rt_task_self(void);
