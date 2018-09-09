#include <rt/rt.h>

#include <rt/context.h>
#include <rt/critical.h>
#include <rt/port.h>
#include <rt/delay.h>

static bool rt_request_exit = false;

static void idle_task_fn(size_t argc, uintptr_t *argv)
{
  (void)argc;
  (void)argv;

  while (!rt_request_exit)
  {
    rt_yield();
  }
}

static struct rt_task idle_task = {
    .cfg =
        {
            .fn = idle_task_fn,
            .argc = 0,
            .argv = NULL,
            .stack = NULL, // idle runs on the main stack
            .stack_size = 0,
            .name = "idle",
            .priority = 0,
        },
};

static uint_fast8_t critical_nesting = 0;

void rt_critical_begin(void)
{
  rt_disable_interrupts();
  ++critical_nesting;
}

void rt_critical_end(void)
{
  --critical_nesting;
  if (critical_nesting == 0)
  {
    rt_enable_interrupts();
  }
}

static LIST_HEAD(ready_list);
static struct rt_task *active_task = &idle_task;

struct rt_task *rt_self(void)
{
  return active_task;
}

void rt_sched(void)
{
  rt_critical_begin();
  struct rt_task *old = active_task;
  active_task = list_item(list_front(&ready_list), struct rt_task, list);
  list_del(&active_task->list);
  const uint_fast8_t saved_nesting = critical_nesting;
  critical_nesting = 0;
  rt_context_swap(&old->ctx, &active_task->ctx);
  critical_nesting = saved_nesting;
  rt_critical_end();
}

void rt_yield(void)
{
  rt_critical_begin();
  list_add_tail(&ready_list, &active_task->list);
  rt_sched();
  rt_critical_end();
}

void rt_suspend(struct rt_task *task)
{
  if (task == active_task)
  {
    rt_sched();
  }
  else
  {
    rt_critical_begin();
    list_del(&task->list);
    rt_critical_end();
  }
}

void rt_resume(struct rt_task *task)
{
  if (task == rt_self())
  {
    return;
  }
  rt_critical_begin();
  list_add_tail(&ready_list, &task->list);
  // TODO: deal with different priorities
  rt_critical_end();
}

static void run_task(void *arg)
{
  const struct rt_task *task = arg;
  task->cfg.fn(task->cfg.argc, task->cfg.argv);
  rt_sched();
}

void rt_task_init(struct rt_task *task, const struct rt_task_config *cfg)
{
  task->cfg = *cfg;
  rt_context_init(&task->ctx, cfg->stack, cfg->stack_size, run_task, task);
  rt_resume(task);
}

static rt_tick_t ticks;

void rt_tick(void)
{
  ++ticks;
  rt_delay_wake_tasks();
  rt_yield();
}

rt_tick_t rt_tick_count(void)
{
  return ticks;
}

void rt_start(void)
{
  rt_port_start();
  rt_yield();
  idle_task.cfg.fn(idle_task.cfg.argc, idle_task.cfg.argv);
}

void rt_stop(void)
{
  // TODO: suspend all other tasks and switch to idle
  rt_port_stop();
  rt_request_exit = true;
}
