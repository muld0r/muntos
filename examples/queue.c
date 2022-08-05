#include <rt/critical.h>
#include <rt/sleep.h>
#include <rt/queue.h>
#include <rt/rt.h>

#include <limits.h>
#include <stdio.h>

static uintptr_t queue_buf[16];
static RT_QUEUE_FROM_ARRAY(queue, queue_buf);

static void queue_send_fn(void)
{
    int n = 100;
    for (int x = 0; x < n; ++x)
    {
        rt_queue_send(&queue, &x);
    }
}

static void queue_recv_fn(void)
{
    int n = 100;
    int x;
    for (int i = 0; i < n; ++i)
    {
        rt_queue_recv(&queue, &x);
        rt_critical_begin();
        printf("received %d\n", x);
        fflush(stdout);
        rt_critical_end();
    }
    rt_stop();
}

int main(void)
{
    static char task0_stack[PTHREAD_STACK_MIN], task1_stack[PTHREAD_STACK_MIN];
    static const struct rt_task_config task0_cfg = {
        .fn = queue_send_fn,
        .stack = task0_stack,
        .stack_size = sizeof(task0_stack),
        .name = "task0",
        .priority = 1,
    };
    static const struct rt_task_config task1_cfg = {
        .fn = queue_recv_fn,
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
