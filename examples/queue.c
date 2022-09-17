#include <rt/queue.h>
#include <rt/sleep.h>
#include <rt/rt.h>

#include <limits.h>
#include <stdio.h>

static const int n = 10;
static int queue_buf[2];
static RT_QUEUE_FROM_ARRAY(queue, queue_buf);

static void send_fn(void)
{
    for (int i = 1; i <= n; ++i)
    {
        rt_queue_send(&queue, &i);
    }
}

static void recv_fn(void)
{
    int x;
    for (int i = 1; i <= n; ++i)
    {
        rt_queue_recv(&queue, &x);
        printf("received %d\n", x);
        fflush(stdout);
    }
    rt_stop();
}

int main(void)
{
    static char sender_stack[PTHREAD_STACK_MIN],
        receiver_stack[PTHREAD_STACK_MIN];
    static struct rt_task sender;
    static struct rt_task receiver;
    rt_task_init(&sender, send_fn, sender_stack, sizeof sender_stack, "sender",
                  1);
    rt_task_init(&receiver, recv_fn, receiver_stack, sizeof receiver_stack,
                  "receiver", 1);
    rt_start();
}
