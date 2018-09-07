#include <rt/rt.h>

#include <rt/context.h>
#include <rt/critical.h>
#include <rt/port.h>

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

static LIST_HEAD(ready_list);
static LIST_HEAD(delay_list);
static struct rt_task *active_task = &idle_task;

struct rt_task *rt_self(void)
{
  return active_task;
}

// TODO: coalesce rt_yield and rt_suspend

void rt_yield(void)
{
  rt_critical_begin();
  struct rt_task *old = active_task;
  list_add_tail(&ready_list, &old->list);
  active_task = list_item(list_front(&ready_list), struct rt_task, list);
  list_del(&active_task->list);
  rt_critical_end();
  rt_context_swap(&old->ctx, &active_task->ctx);
}

void rt_suspend(struct rt_task *task)
{
  if (task == active_task)
  {
    rt_critical_begin();
    active_task = list_item(list_front(&ready_list), struct rt_task, list);
    list_del(&active_task->list);
    rt_critical_end();
    // TODO, call context swap in critical section? doesn't work right in
    // pthread port
    rt_context_swap(&task->ctx, &active_task->ctx);
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
  rt_suspend(rt_self());
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
  for (;;)
  {
    struct rt_task *task =
        list_item(list_front(&delay_list), struct rt_task, list);
    if (ticks != task->delay_until)
    {
      break;
    }
    list_del(&task->list);
    rt_resume(task);
  }
  rt_yield();
}

rt_tick_t rt_tick_count(void)
{
  return ticks;
}

void rt_delay(rt_tick_t ticks_to_delay)
{
  if (ticks_to_delay == 0)
  {
    return;
  }
  struct rt_task *self = rt_self();
  if (ticks_to_delay == RT_TICK_MAX)
  {
    rt_suspend(self);
    return;
  }

  self->delay_until = rt_tick_count() + ticks_to_delay;

  struct list *delay_insert;
  rt_critical_begin();
  for (delay_insert = delay_list.next; delay_insert != &delay_list;
       delay_insert = delay_insert->next)
  {
    const struct rt_task *task =
        list_item(delay_insert, struct rt_task, list);
    if (task->delay_until - rt_tick_count() > ticks_to_delay)
    {
      break;
    }
  }
  list_add_tail(delay_insert, &self->list);
  rt_critical_end();
  rt_suspend(self);
}

void rt_start(void)
{
  rt_port_start();
  idle_task.cfg.fn(idle_task.cfg.argc, idle_task.cfg.argv);
}

void rt_stop(void)
{
  // TODO: suspend all other tasks and switch to idle
  rt_port_stop();
  rt_request_exit = true;
}

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
