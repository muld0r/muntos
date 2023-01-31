#include <rt/cycle.h>
#include <rt/log.h>
#include <rt/queue.h>
#include <rt/rt.h>
#include <rt/task.h>

static volatile uint32_t start_cycle = 0;
static volatile uint32_t cycles = 0;

RT_QUEUE_STATIC(queue, int, 10);

static void waiter(void)
{
    int x;
    rt_queue_pop(&queue, &x);
    cycles = rt_cycle() - start_cycle;
    rt_stop();
}

static void poster(void)
{
    int x = 0;
    start_cycle = rt_cycle();
    rt_queue_push(&queue, &x);
}

int main(void)
{
    static char task_stacks[2][TASK_STACK_SIZE]
        __attribute__((aligned(STACK_ALIGN)));

    RT_TASK(waiter, task_stacks[0], 2);
    RT_TASK(poster, task_stacks[1], 1);

    rt_cycle_enable();
    rt_start();

    rt_logf("cycles = %u\n", cycles);
}
