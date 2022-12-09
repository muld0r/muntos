#include <rt/log.h>
#include <rt/queue.h>
#include <rt/rt.h>
#include <rt/sleep.h>
#include <rt/task.h>

RT_QUEUE_STATIC(queue, int, 20);

static void sender(uintptr_t offset)
{
    int i = (int)offset;
    for (;;)
    {
        rt_queue_send(&queue, &i);
        ++i;
    }
}

static bool out_of_order = false;
static int max_elem[100] = {0};

static void receiver(void)
{
    int x[5];
    for (;;)
    {
        for (int i = 0; i < 5; ++i)
        {
            rt_queue_recv(&queue, &x[i]);
            int task = x[i] / 1000000;
            int elem = x[i] % 1000000;
            if (elem < max_elem[task])
            {
                out_of_order = true;
            }
            max_elem[task] = elem;
        }
    }
}

static void timeout(void)
{
    rt_sleep(1000);
    rt_stop();
}

int main(void)
{
    static char sender_stacks[100][TASK_STACK_SIZE]
        __attribute__((aligned(STACK_ALIGN)));
    static struct rt_task senders[100];

    for (int i = 0; i < 100; ++i)
    {
        rt_task_init_arg(&senders[i], sender, (uintptr_t)i * 1000000, "sender",
                         2, sender_stacks[i], TASK_STACK_SIZE);
    }

    static char receiver_stack[TASK_STACK_SIZE]
        __attribute__((aligned(STACK_ALIGN)));
    RT_TASK(receiver, receiver_stack, 1);

    static char timeout_stack[TASK_STACK_SIZE]
        __attribute__((aligned(STACK_ALIGN)));
    RT_TASK(timeout, timeout_stack, 0);
    rt_start();

    if (out_of_order)
    {
        return 1;
    }

    for (size_t i = 0; i < queue.num_elems; ++i)
    {
        rt_logf("%02x ",
                rt_atomic_load_explicit(&queue.slots[i], memory_order_relaxed));
    }
    rt_logf("\n");
}
