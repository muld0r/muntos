#include <rt/rt.h>

#include <rt/context.h>

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
  list_add_tail(task_list, &task->list);
  // TODO: deal with different priorities
}

void rt_start(void)
{
  idle_task.cfg.fn(idle_task.cfg.argc, idle_task.cfg.argv);
}

// TODO: refactor rt_suspend, rt_yield, and rt_exit to call rt_switch
// special cases of a rt_switch

void rt_suspend(void)
{
  // TODO
  // rt.active_task->runnable = false;
  rt_yield();
}

void rt_yield(void)
{
  struct rt_task *old = list_item(task_list, struct rt_task, list);
  task_list = task_list->next;
  const struct rt_task *new = list_item(task_list, struct rt_task, list);
  rt_context_swap(&old->ctx, &new->ctx);
}
