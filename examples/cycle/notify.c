#include <rt/cycle.h>
#include <rt/log.h>
#include <rt/notify.h>
#include <rt/rt.h>
#include <rt/task.h>

static volatile uint32_t start_cycle = 0;
static volatile uint32_t cycles = 0;

static RT_NOTIFY(note, 0);

static void waiter(void)
{
    rt_notify_wait(&note);
    cycles = rt_cycle() - start_cycle;
    rt_stop();
}

static void poster(void)
{
    start_cycle = rt_cycle();
    rt_notify(&note);
}

int main(void)
{
    RT_TASK(waiter, RT_STACK_MIN, 2);
    RT_TASK(poster, RT_STACK_MIN, 1);

    rt_start();

    rt_logf("cycles = %u\n", (unsigned)cycles);
}
