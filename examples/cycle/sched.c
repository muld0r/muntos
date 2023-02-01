#include <rt/cycle.h>
#include <rt/log.h>
#include <rt/rt.h>
#include <rt/task.h>

static volatile uint32_t start_cycle = 0;
static volatile uint32_t cycles = 0;

static void task0(void)
{
    start_cycle = rt_cycle();
    rt_sched();
}

static void task1(void)
{
    cycles = rt_cycle() - start_cycle;
    rt_stop();
}

int main(void)
{
    static char task_stacks[2][TASK_STACK_SIZE]
        __attribute__((aligned(STACK_ALIGN)));

    RT_TASK(task0, task_stacks[0], 1);
    RT_TASK(task1, task_stacks[1], 1);

    rt_cycle_enable();
    rt_start();

    rt_logf("cycles = %u\n", (unsigned)cycles);
}
