#include <rt/log.h>
#include <rt/queue.h>
#include <rt/rt.h>
#include <rt/sleep.h>
#include <rt/task.h>

#include <stdint.h>

RT_QUEUE_STATIC(queue, uint32_t, 10);

static void pusher(uintptr_t i)
{
    for (;;)
    {
        rt_queue_push(&queue, &i);
        ++i;
    }
}

#define NPUSHERS 4
#define TASK_INC UINT32_C(0x1000000)

static volatile bool out_of_order = false;
static uint32_t max_elem[NPUSHERS] = {0};
static volatile size_t num_popped = 0;

static void popper(void)
{
    uint32_t x;
    for (;;)
    {
        rt_queue_pop(&queue, &x);
        ++num_popped;
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
    static char pusher_stacks[NPUSHERS][TASK_STACK_SIZE]
        __attribute__((aligned(STACK_ALIGN)));
    static struct rt_task pushers[NPUSHERS];

    for (uintptr_t i = 0; i < NPUSHERS; ++i)
    {
        rt_task_init_arg(&pushers[i], pusher, i * TASK_INC, "pusher", 1,
                         pusher_stacks[i], TASK_STACK_SIZE);
    }

    static char popper_stack[TASK_STACK_SIZE]
        __attribute__((aligned(STACK_ALIGN)));
    RT_TASK(popper, popper_stack, 2);

    static char timeout_stack[TASK_STACK_SIZE]
        __attribute__((aligned(STACK_ALIGN)));
    RT_TASK(timeout, timeout_stack, UINT_MAX);
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
