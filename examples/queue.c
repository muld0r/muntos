#include <rt/critical.h>
#include <rt/sleep.h>
#include <rt/queue.h>
#include <rt/rt.h>

#include <limits.h>
#include <stdio.h>

static const int n = 1000;
static int queue_buf[16];
static RT_QUEUE_FROM_ARRAY(queue, queue_buf);

static void queue_send_fn(void)
{
    for (int i = 1; i <= n; ++i)
    {
        rt_queue_send(&queue, &i);
    }
}

static void queue_recv_fn(void)
{
    int x;
    for (int i = 1; i <= n; ++i)
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
    static char sender_stack[PTHREAD_STACK_MIN], receiver_stack[PTHREAD_STACK_MIN];
    static const struct rt_task_config sender_cfg = {
        .fn = queue_send_fn,
        .stack = sender_stack,
        .stack_size = sizeof(sender_stack),
        .name = "sender",
        .priority = 1,
    };
    static const struct rt_task_config receiver_cfg = {
        .fn = queue_recv_fn,
        .stack = receiver_stack,
        .stack_size = sizeof(receiver_stack),
        .name = "receiver",
        .priority = 1,
    };
    static struct rt_task sender, receiver;
    rt_task_init(&sender, &sender_cfg);
    rt_task_init(&receiver, &receiver_cfg);

    rt_start();
}
