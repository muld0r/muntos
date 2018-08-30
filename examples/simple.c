#include <rt/critical.h>
#include <rt/rt.h>

#include <stdio.h>
#include <unistd.h>

__attribute__((noreturn)) static void simple_fn(size_t argc, uintptr_t *argv)
{
  (void)argc;
  (void)argv;
  for (;;)
  {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    rt_critical_begin();
    printf("%s %lu %ld\n", rt_self()->cfg.name, argv[0],
           ts.tv_sec * 1000000 + ts.tv_nsec / 1000);
    rt_critical_end();
    ++argv[0];
  }
}

int main(void)
{
  static uintptr_t x, y;
  static char task0_stack[2048];
  static const struct rt_task_config task0_cfg = {
      .fn = simple_fn,
      .argc = 1,
      .argv = &x,
      .stack = task0_stack,
      .stack_size = sizeof(task0_stack),
      .name = "task0",
      .priority = 1,
  };
  static char task1_stack[2048];
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
