#include <rt/queue.h>
#include <rt/rt.h>
#include <rt/task.h>
#include <rt/sleep.h>

static const int n = 1000;

static void sender(void *arg)
{
    struct rt_queue *queue = arg;
    for (int i = 0; i < n; ++i)
    {
        rt_queue_send(queue, &i);
    }
}

static volatile bool out_of_order = false;

static void receiver(void *arg)
{
    struct rt_queue *queue = arg;
    int x;
    for (int i = 0; i < n; ++i)
    {
        rt_queue_recv(queue, &x);
        if (x != i)
        {
            out_of_order = true;
        }
    }
    rt_stop();
}

static volatile bool timed_out = false;

static void timeout(void *arg)
{
    (void)arg;
    rt_sleep(500);
    timed_out = true;
    rt_stop();
}

int main(void)
{
    RT_QUEUE_STATIC(queue, int, 5);
    __attribute__((aligned(STACK_ALIGN))) static char
        sender_stack[TASK_STACK_SIZE],
        receiver_stack[TASK_STACK_SIZE],
        timeout_stack[TASK_STACK_SIZE];
    RT_TASK(sender, &queue, sender_stack, 2);
    RT_TASK(receiver, &queue, receiver_stack, 2);
    RT_TASK(timeout, NULL, timeout_stack, 1);
    rt_start();
    if (out_of_order)
    {
        return 1;
    }
    if (timed_out)
    {
        return 2;
    }
}
