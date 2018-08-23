#include <rt/rt.h>

#include <rt/context.h>

struct rt
{
  struct list ready_list;
  struct rt_task *active_task;
};

static struct rt rt = {
    .ready_list = LIST_HEAD_INIT(rt.ready_list),
    .active_task = NULL,
};

static void run_task(void *arg)
{
  const struct rt_task *task = arg;
  task->cfg.fn(task->cfg.argc, task->cfg.argv);
  // TODO: clean up/disable context/task when fn returns
  rt_suspend();
}

void rt_task_init(struct rt_task *task, const struct rt_task_config *cfg)
{
  task->cfg = *cfg;
  context_init(&task->ctx, cfg->stack, cfg->stack_size, run_task, task);
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

__attribute__((noreturn)) void rt_start(void)
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
  rt.active_task =
      list_item(list_pop_front(&rt.ready_list), struct rt_task, list_node);
  context_restore(&rt.active_task->ctx);

  // TODO
  for (;;)
  {
    rt_yield();
  }
}

void rt_suspend(void)
{
  rt.active_task->runnable = false;
  rt_yield();
}

void rt_yield(void)
{
  context_save(&rt.active_task->ctx);
  list_add_tail(&rt.ready_list, &rt.active_task->list_node);
  rt.active_task =
      list_item(list_pop_front(&rt.ready_list), struct rt_task, list_node);
  context_restore(&rt.active_task->ctx);
}
