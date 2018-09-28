#include <rt/critical.h>
#include <rt/delay.h>
#include <rt/queue.h>
#include <rt/rt.h>

#include <stdio.h>
#include <limits.h>

static uintptr_t queue_buf[16];
static rt_queue_t queue = RT_QUEUE_ARRAY_INIT(queue, queue_buf);

static void queue_send_fn(size_t argc, const uintptr_t *argv)
{
  (void)argc;
  uintptr_t n = argv[0];
  for (uintptr_t x = 0; x < n; ++x)
  {
    rt_queue_send(&queue, &x, RT_TICK_MAX);
  }
}

static void queue_recv_fn(size_t argc, const uintptr_t *argv)
{
  (void)argc;
  uintptr_t n = argv[0];
  uintptr_t x;
  for (uintptr_t i = 0; i < n; ++i)
  {
    rt_queue_recv(&queue, &x, RT_TICK_MAX);
    rt_critical_begin();
    printf("received %zu\n", x);
    fflush(stdout);
    rt_critical_end();
  }
  rt_stop();
}

int main(void)
{
  static char task0_stack[PTHREAD_STACK_MIN], task1_stack[PTHREAD_STACK_MIN];
  static const uintptr_t n = 100;
  static const struct rt_task_config task0_cfg = {
      .fn = queue_send_fn,
      .argc = 1,
      .argv = &n,
      .stack = task0_stack,
      .stack_size = sizeof(task0_stack),
      .name = "task0",
      .priority = 1,
  };
  static const struct rt_task_config task1_cfg = {
      .fn = queue_recv_fn,
      .argc = 1,
      .argv = &n,
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
