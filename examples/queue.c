#include <rt/log.h>
#include <rt/queue.h>
#include <rt/rt.h>
#include <rt/sleep.h>
#include <rt/task.h>

static const int n = 1000;

static void sender(void *arg)
{
    struct rt_queue *queue = arg;
    for (int i = 0; i < n; ++i)
    {
        rt_logf("sender: %d\n", i);
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
        rt_logf("receiver: %d, %d\n", i, x);
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
    static char task_stacks[3][TASK_STACK_SIZE]
        __attribute__((aligned(STACK_ALIGN)));
    RT_TASK(sender, &queue, task_stacks[0], 2);
    RT_TASK(receiver, &queue, task_stacks[1], 2);
    RT_TASK(timeout, NULL, task_stacks[2], 1);
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
