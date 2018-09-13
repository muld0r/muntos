#include <rt/critical.h>
#include <rt/rt.h>

#include <stdio.h>
#include <limits.h>

static void simple_fn(size_t argc, uintptr_t *argv)
{
  if (argc < 1)
  {
    return;
  }

  while (argv[0] > 0)
  {
    rt_critical_begin();
    printf("%s %lu, tick %u\n", rt_self()->cfg.name, argv[0],
           rt_tick_count());
    fflush(stdout);
    rt_critical_end();
    --argv[0];
  }
  rt_stop();
}

int main(void)
{
  static uintptr_t x = 100000, y = 100000;
  static char task0_stack[PTHREAD_STACK_MIN], task1_stack[PTHREAD_STACK_MIN];
  static const struct rt_task_config task0_cfg = {
      .fn = simple_fn,
      .argc = 1,
      .argv = &x,
      .stack = task0_stack,
      .stack_size = sizeof(task0_stack),
      .name = "task0",
      .priority = 1,
  };
  static const struct rt_task_config task1_cfg = {
      .fn = simple_fn,
      .argc = 1,
      .argv = &y,
      .stack = task1_stack,
      .stack_size = sizeof(task1_stack),
      .name = "task1",
      .priority = 1,
  };
  static struct rt_task task0, task1;
  rt_task_init(&task0, &task0_cfg);
  rt_task_init(&task1, &task1_cfg);

  rt_start();
}
