#include <rt/queue.h>
#include <rt/rt.h>

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
    }
    rt_stop();
}

int main(void)
{
    static int queue_buf[2];
    static RT_QUEUE_FROM_ARRAY(queue, queue_buf);
    __attribute__((aligned(STACK_ALIGN))) static char
        sender_stack[TASK_STACK_SIZE],
        receiver_stack[TASK_STACK_SIZE];
    RT_TASK(sender, &queue, sender_stack, 1);
    RT_TASK(receiver, &queue, receiver_stack, 1);
    rt_start();
}
