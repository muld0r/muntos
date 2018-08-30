#include <rt/rt.h>

#include <stdio.h>
#include <unistd.h>

static void simple_fn(size_t argc, uintptr_t *argv)
{
  (void)argc;
  (void)argv;
  unsigned int x = 0;
  while (x < 1000)
  {
    printf("%u\n", x);
    rt_yield();
    ++x;
  }
}

int main(void)
{
  static char task_stack[2048];
  static const struct rt_task_config task_cfg = {
      .fn = simple_fn,
      .argc = 0,
      .argv = NULL,
      .stack = task_stack,
      .stack_size = sizeof(task_stack),
      .name = "simple",
      .priority = 1,
  };
  static struct rt_task task;
  rt_task_init(&task, &task_cfg);

  rt_start();
}
