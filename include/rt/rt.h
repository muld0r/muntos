#pragma once

#include <rt/context.h>

#include <rt/list.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef unsigned int rt_priority_t;

typedef uint32_t rt_tick_t;
#define RT_TICK_MAX ((rt_tick_t)UINT32_MAX)

struct rt_task_config
{
  void (*fn)(size_t argc, uintptr_t *argv);
  size_t argc;
  uintptr_t *argv;
  void *stack;
  size_t stack_size;
  const char *name;
  rt_priority_t priority;
};

struct rt_task
{
  struct list list;
  struct rt_task_config cfg;
  rt_context_t ctx;
  rt_tick_t delay_until;
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
 * Suspend a task.
 */
void rt_suspend(struct rt_task *task);

/*
 * Resume a task.
 */
void rt_resume(struct rt_task *task);

/*
 * Get a pointer to the current task.
 */
struct rt_task *rt_self(void);

/*
 * Run a tick. Should be called periodically.
 */
void rt_tick(void);

/*
 * Return the current tick number.
 */
rt_tick_t rt_tick_count(void);

/*
 * Delay the current task for a given number of ticks.
 */
void rt_delay(rt_tick_t ticks_to_delay);
