#include <rt/critical.h>
#include <rt/queue.h>
#include <rt/sleep.h>
#include <rt/rt.h>

#include <limits.h>
#include <stdio.h>

static const int n = 1000;
static int queue_buf[16];
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
        rt_critical_begin();
        printf("received %d\n", x);
        fflush(stdout);
        rt_critical_end();
    }
    rt_stop();
}

int main(void)
{
    static char sender_stack[PTHREAD_STACK_MIN],
        receiver_stack[PTHREAD_STACK_MIN];
    static RT_TASK(sender, send_fn, sender_stack, 1);
    static RT_TASK(receiver, recv_fn, receiver_stack, 1);
    rt_task_launch(&sender);
    rt_task_launch(&receiver);

    rt_start();
}
