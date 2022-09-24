#include <rt/queue.h>
#include <rt/sleep.h>
#include <rt/rt.h>

#include <limits.h>
#include <stdio.h>

static const int n = 10;

static void sender(void *arg)
{
    struct rt_queue *queue = arg;
    for (int i = 1; i <= n; ++i)
    {
        rt_queue_send(queue, &i);
    }
}

static void receiver(void *arg)
{
    struct rt_queue *queue = arg;
    int x;
    for (int i = 1; i <= n; ++i)
    {
        rt_queue_recv(queue, &x);
        printf("received %d\n", x);
        fflush(stdout);
    }
    rt_stop();
}

int main(void)
{
    static int queue_buf[2];
    static RT_QUEUE_FROM_ARRAY(queue, queue_buf);
    static char sender_stack[PTHREAD_STACK_MIN],
        receiver_stack[PTHREAD_STACK_MIN];
    RT_TASK(sender, &queue, sender_stack, 1);
    RT_TASK(receiver, &queue, receiver_stack, 1);
    rt_start();
}
