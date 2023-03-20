#include <rt/cycle.h>
#include <rt/log.h>
#include <rt/queue.h>
#include <rt/rt.h>
#include <rt/task.h>

static volatile uint32_t start_cycle = 0;
static volatile uint32_t cycles = 0;

RT_QUEUE_STATIC(queue, int, 10);

static void popper(void)
{
    int x;
    rt_queue_pop(&queue, &x);
    cycles = rt_cycle() - start_cycle;
    rt_stop();
}

static void pusher(void)
{
    int x = 0;
    start_cycle = rt_cycle();
    rt_queue_push(&queue, &x);
}

int main(void)
{
    RT_TASK(popper, RT_STACK_MIN, 2);
    RT_TASK(pusher, RT_STACK_MIN, 1);

    rt_start();

    rt_logf("cycles = %u\n", (unsigned)cycles);
}
