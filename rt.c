#include <rt/rt.h>

#include <rt/context.h>

struct rt
{
  struct list ready_list;
  struct rt_task *active_task;
};

static inline struct rt_task *task_from_node(struct list *node)
{
  return list_item(node, struct rt_task, list_node);
}

static struct rt rt = {
    .ready_list = LIST_HEAD_INIT(rt.ready_list),
    .active_task = NULL,
};

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
  static char idle_task_stack[RT_CONTEXT_STACK_MIN];
  static const struct rt_task_config idle_task_cfg = {
      .fn = idle_task_fn,
      .argc = 0,
      .argv = NULL,
      .stack = idle_task_stack,
      .stack_size = sizeof(idle_task_stack),
      .name = "idle",
      .priority = 0,
  };
  static struct rt_task idle_task;
  rt_task_init(&idle_task, &idle_task_cfg);
  rt.active_task = task_from_node(list_pop_front(&rt.ready_list));
  rt_context_t main_context;
  rt_context_swap(&main_context, &rt.active_task->ctx);
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
  struct rt_task *old = rt.active_task;
  struct rt_task *new = task_from_node(list_pop_front(&rt.ready_list));
  list_add_tail(&rt.ready_list, &old->list_node);
  rt.active_task = new;
  rt_context_swap(&old->ctx, &new->ctx);
}
