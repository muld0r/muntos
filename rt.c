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

static uint_fast8_t critical_save(void)
{
  uint_fast8_t nesting = critical_nesting;
  critical_nesting = 0;
  rt_enable_interrupts();
  return nesting;
}

static void critical_restore(uint_fast8_t nesting)
{
  rt_disable_interrupts();
  critical_nesting = nesting;
}

static void swap(rt_context_t *old_ctx, const rt_context_t *new_ctx)
{
  uint_fast8_t nesting = critical_save();
  rt_context_swap(old_ctx, new_ctx);
  critical_restore(nesting);
}

static LIST_HEAD(ready_list);
static LIST_HEAD(delay_list);
static struct rt_task *active_task = &idle_task;

struct rt_task *rt_self(void)
{
  return active_task;
}

void rt_suspend(struct rt_task *task)
{
  if (task == active_task)
  {
    rt_critical_begin();
    active_task = list_item(list_front(&ready_list), struct rt_task, list);
    list_del(&active_task->list);
    swap(&task->ctx, &active_task->ctx);
    rt_critical_end();
  }
  else
  {
    rt_critical_begin();
    list_del(&task->list);
    rt_critical_end();
  }
}

void rt_yield(void)
{
  rt_critical_begin();
  list_add_tail(&ready_list, &active_task->list);
  rt_suspend(active_task);
  rt_critical_end();
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
    if (ticks != task->wake_tick)
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

static void delay_until(rt_tick_t wake_tick, rt_tick_t max_delay)
{
  rt_critical_begin();
  const rt_tick_t ticks_until_wake = wake_tick - rt_tick_count();
  if (0 < ticks_until_wake && ticks_until_wake <= max_delay)
  {
    struct list *delay_insert;
    for (delay_insert = list_front(&delay_list); delay_insert != &delay_list;
         delay_insert = delay_insert->next)
    {
      const struct rt_task *task =
          list_item(delay_insert, struct rt_task, list);
      if (task->wake_tick - rt_tick_count() > ticks_until_wake)
      {
        break;
      }
    }
    active_task->wake_tick = wake_tick;
    list_add_tail(delay_insert, &active_task->list);
    rt_suspend(active_task);
  }
  rt_critical_end();
}

void rt_delay(rt_tick_t delay)
{
  delay_until(rt_tick_count() + delay, delay);
}

void rt_delay_periodic(rt_tick_t *last_wake_tick, rt_tick_t period)
{
  delay_until(*last_wake_tick += period, period);
}
