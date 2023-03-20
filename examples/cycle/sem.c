#include <rt/cycle.h>
#include <rt/log.h>
#include <rt/rt.h>
#include <rt/sem.h>
#include <rt/task.h>

static volatile uint32_t start_cycle = 0;
static volatile uint32_t cycles = 0;

static RT_SEM(sem, 0);

static void waiter(void)
{
    rt_sem_wait(&sem);
    cycles = rt_cycle() - start_cycle;
    rt_stop();
}

static void poster(void)
{
    start_cycle = rt_cycle();
    rt_sem_post(&sem);
}

int main(void)
{
    RT_TASK(waiter, RT_STACK_MIN, 2);
    RT_TASK(poster, RT_STACK_MIN, 1);

    rt_start();

    rt_logf("cycles = %u\n", (unsigned)cycles);
}
