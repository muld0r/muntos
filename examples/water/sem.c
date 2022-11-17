#include "water.h"

#include <rt/mutex.h>
#include <rt/sem.h>

struct reaction
{
    struct rt_sem hready, hdone;
    struct rt_mutex m;
};

static struct reaction rxn = {
    .hready = RT_SEM_INIT(rxn.hready, 0),
    .hdone = RT_SEM_INIT(rxn.hdone, 0),
    .m = RT_MUTEX_INIT(rxn.m),
};

void hydrogen(void)
{
    rt_sem_post(&rxn.hready);
    rt_sem_wait(&rxn.hdone);
}

void oxygen(void)
{
    rt_mutex_lock(&rxn.m);
    rt_sem_wait(&rxn.hready);
    rt_sem_wait(&rxn.hready);
    rt_mutex_unlock(&rxn.m);
    make_water();
    rt_sem_post(&rxn.hdone);
    rt_sem_post(&rxn.hdone);
}
