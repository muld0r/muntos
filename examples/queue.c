#include <rt/log.h>
#include <rt/queue.h>
#include <rt/rt.h>
#include <rt/sleep.h>
#include <rt/task.h>

#include <stdint.h>

RT_QUEUE_STATIC(queue, uint32_t, 10);

static void sender(uintptr_t i)
{
    for (;;)
    {
        rt_queue_send(&queue, &i);
        ++i;
    }
}

#define NSENDERS 4
#define TASK_INC UINT32_C(0x1000000)

static volatile bool out_of_order = false;
static uint32_t max_elem[NSENDERS] = {0};
static volatile size_t num_recv = 0;

static void receiver(void)
{
    uint32_t x;
    for (;;)
    {
        rt_queue_recv(&queue, &x);
        ++num_recv;
        uint32_t task = x / TASK_INC;
        uint32_t elem = x % TASK_INC;
        if (elem < max_elem[task])
        {
            out_of_order = true;
        }
        max_elem[task] = elem;
    }
}

static void timeout(void)
{
    rt_sleep(1000);
    rt_stop();
}

int main(void)
{
    static char sender_stacks[NSENDERS][TASK_STACK_SIZE]
        __attribute__((aligned(STACK_ALIGN)));
    static struct rt_task senders[NSENDERS];

    for (uintptr_t i = 0; i < NSENDERS; ++i)
    {
        rt_task_init_arg(&senders[i], sender, i * TASK_INC, "sender",
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
