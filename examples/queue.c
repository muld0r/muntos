#include <rt/queue.h>
#include <rt/sleep.h>
#include <rt/rt.h>

#include <limits.h>
#include <stdio.h>

static const int n = 10;
static int queue_buf[2];
static RT_QUEUE_FROM_ARRAY(queue, queue_buf);

static void sender(void)
{
    for (int i = 1; i <= n; ++i)
    {
        rt_queue_send(&queue, &i);
    }
}

static void receiver(void)
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
    RT_TASK(sender, sender_stack, 1);
    RT_TASK(receiver, receiver_stack, 1);
    rt_start();
}
