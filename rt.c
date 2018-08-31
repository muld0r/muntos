#include <rt/rt.h>

#include <rt/context.h>
#include <rt/critical.h>
#include <rt/port.h>

__attribute__((noreturn)) static void idle_task_fn(size_t argc, uintptr_t *argv)
{
  (void)argc;
  (void)argv;

  for (;;)
  {
    rt_yield();
  }
}

static struct rt_task idle_task = {
    .list = LIST_HEAD_INIT(idle_task.list),
    .cfg =
        {
            .fn = idle_task_fn,
            .argc = 0,
            .argv = NULL,
            .stack = NULL,
            .stack_size = 0,
            .name = "idle",
            .priority = 0,
        },
    .runnable = true,
};

static struct list *task_list = &idle_task.list;

struct rt_task *rt_self(void)
{
  return list_item(task_list, struct rt_task, list);
}

static inline void cycle_tasks(void)
{
  task_list = task_list->next;
}

void rt_yield(void)
{
  rt_critical_begin();
  struct rt_task *old = rt_self();
  cycle_tasks();
  struct rt_task *new = rt_self();
  rt_critical_end();
  rt_context_swap(&old->ctx, &new->ctx);
}

void rt_suspend(struct rt_task *task)
{
  if (task == rt_self())
  {
    rt_critical_begin();
    cycle_tasks();
    struct rt_task *new_task = rt_self();
    list_del(&task->list);
    rt_critical_end();
    rt_context_swap(&task->ctx, &new_task->ctx);
  }
  else
  {
    rt_critical_begin();
    task->runnable = false;
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
  task->runnable = true;
  list_add_tail(task_list, &task->list);
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
  task->runnable = true;
  list_add_tail(task_list, &task->list);
  // TODO: deal with different priorities
}

void rt_tick(void)
{
  rt_yield();
}

void rt_start(void)
{
  rt_port_start();
  idle_task.cfg.fn(idle_task.cfg.argc, idle_task.cfg.argv);
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
