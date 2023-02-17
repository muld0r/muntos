#include "water.h"

#include <rt/barrier.h>
#include <rt/sem.h>

struct reaction
{
    struct rt_sem h, o;
    struct rt_barrier barrier;
};

static struct reaction rxn = {
    .h = RT_SEM_INIT(rxn.h, 2),
    .o = RT_SEM_INIT(rxn.o, 1),
    .barrier = RT_BARRIER_INIT(rxn.barrier, 3),
};

void hydrogen(void)
{
    rt_sem_wait(&rxn.h);
    rt_barrier_wait(&rxn.barrier);
    rt_barrier_wait(&rxn.barrier);
    rt_sem_post(&rxn.h);
}

void oxygen(void)
{
    rt_sem_wait(&rxn.o);
    rt_barrier_wait(&rxn.barrier);
    make_water();
    rt_barrier_wait(&rxn.barrier);
    rt_sem_post(&rxn.o);
}
