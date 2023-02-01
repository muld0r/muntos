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
    static char task_stacks[2][TASK_STACK_SIZE]
        __attribute__((aligned(STACK_ALIGN)));

    RT_TASK(waiter, task_stacks[0], 2);
    RT_TASK(poster, task_stacks[1], 1);

    rt_cycle_enable();
    rt_start();

    rt_logf("cycles = %u\n", (unsigned)cycles);
}
