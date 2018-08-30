#pragma once

#include <rt/context.h>

#include <rt/list.h>

#include <stddef.h>
#include <stdint.h>

/*
 * Start the rt scheduler.
 */
void rt_start(void);

typedef unsigned int rt_priority_t;

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
  bool runnable;
};

/*
 * Initialize a task.
 */
void rt_task_init(struct rt_task *task, const struct rt_task_config *cfg);

/*
 * Yield control of the processor to another runnable task.
 */
void rt_yield(void);

/*
 * Suspend the current task.
 */
void rt_suspend(void);

/*
 * Get a pointer to the current task.
 */
struct rt_task *rt_self(void);
