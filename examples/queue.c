#include <rt/critical.h>
#include <rt/delay.h>
#include <rt/queue.h>
#include <rt/rt.h>

#include <stdio.h>

static struct rt_queue queue;

static void queue_send_fn(size_t argc, uintptr_t *argv)
{
  (void)argc;
  (void)argv;
  for (unsigned int x = 0; x < 100; ++x)
  {
    rt_queue_send(&queue, &x, 10);
  }
}

static void queue_recv_fn(size_t argc, uintptr_t *argv)
{
  (void)argc;
  (void)argv;
  unsigned int x;
  while (rt_queue_recv(&queue, &x, 10))
  {
    rt_critical_begin();
    printf("received %u\n", x);
    fflush(stdout);
    rt_critical_end();
  }
}

int main(void)
{
  static char task0_stack[RT_STACK_MIN], task1_stack[RT_STACK_MIN];
  static const struct rt_task_config task0_cfg = {
      .fn = queue_send_fn,
      .argc = 0,
      .argv = NULL,
      .stack = task0_stack,
      .stack_size = sizeof(task0_stack),
      .name = "task0",
      .priority = 1,
  };
  static const struct rt_task_config task1_cfg = {
      .fn = queue_recv_fn,
      .argc = 0,
      .argv = NULL,
      .stack = task1_stack,
      .stack_size = sizeof(task1_stack),
      .name = "task1",
      .priority = 1,
  };
  static struct rt_task task0, task1;
  rt_task_init(&task0, &task0_cfg);
  rt_task_init(&task1, &task1_cfg);

  static unsigned int queue_buf[16];
  static const struct rt_queue_config queue_cfg = {
      .buf = queue_buf,
      .elem_size = sizeof(unsigned int),
      .num_elems = 16,
  };
  rt_queue_init(&queue, &queue_cfg);

  rt_start();
}
