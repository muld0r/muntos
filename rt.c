#include <rt/rt.h>

#include <rt/context.h>

static struct rt
{
  struct list ready_list;
  struct rt_task *active_task;
} rt = {
    .ready_list = LIST_HEAD_INIT(rt.ready_list),
    .active_task = NULL,
};

static inline struct rt_task *task_from_node(struct list *node)
{
  return list_item(node, struct rt_task, list_node);
}

static void run_task(void *arg)
{
  const struct rt_task *task = arg;
  task->cfg.fn(task->cfg.argc, task->cfg.argv);
  // TODO: clean up/disable context/task when fn returns
}

void rt_task_init(struct rt_task *task, const struct rt_task_config *cfg)
{
  task->cfg = *cfg;
  rt_context_init(&task->ctx, cfg->stack, cfg->stack_size, run_task, task);
  task->runnable = true;
  list_add_tail(&rt.ready_list, &task->list_node);
  // TODO: deal with different priorities
}

__attribute__((noreturn)) static void idle_task_fn(size_t argc, uintptr_t *argv)
{
  (void)argc;
  (void)argv;

  for (;;)
  {
    rt_yield();
  }
}

void rt_start(void)
{
  static struct rt_task idle_task = {
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
  rt.active_task = &idle_task;
  idle_task.cfg.fn(idle_task.cfg.argc, idle_task.cfg.argv);
}

// TODO: refactor rt_suspend, rt_yield, and rt_exit to call rt_switch
// special cases of a rt_switch

void rt_suspend(void)
{
  // TODO
  rt.active_task->runnable = false;
  rt_yield();
}

void rt_yield(void)
{
  if (list_empty(&rt.ready_list))
  {
    return;
  }
  struct rt_task *old = rt.active_task;
  struct rt_task *new = task_from_node(list_pop_front(&rt.ready_list));
  list_add_tail(&rt.ready_list, &old->list_node);
  rt.active_task = new;
  rt_context_swap(&old->ctx, &new->ctx);
}
