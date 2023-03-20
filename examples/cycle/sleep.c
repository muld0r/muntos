#include <rt/cycle.h>
#include <rt/log.h>
#include <rt/rt.h>
#include <rt/sleep.h>
#include <rt/task.h>

static volatile uint32_t start_cycle = 0;
static volatile uint32_t cycles = 0;

static void sleep(void)
{
    start_cycle = rt_cycle();
    rt_sleep(1);
}

static void task1(void)
{
    cycles = rt_cycle() - start_cycle;
    rt_stop();
}

int main(void)
{
    RT_TASK(sleep, RT_STACK_MIN, 2);
    RT_TASK(task1, RT_STACK_MIN, 1);

    rt_start();

    rt_logf("cycles = %u\n", (unsigned)cycles);
}
