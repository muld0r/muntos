#include <rt/queue.h>
#include <rt/rt.h>
#include <rt/task.h>

static const int n = 1000;

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
    RT_QUEUE_STATIC(queue, int, 40);
    __attribute__((aligned(STACK_ALIGN))) static char
        sender_stack[TASK_STACK_SIZE],
        receiver_stack[TASK_STACK_SIZE];
    RT_TASK(sender, &queue, sender_stack, 1);
    RT_TASK(receiver, &queue, receiver_stack, 1);
    rt_start();
}
