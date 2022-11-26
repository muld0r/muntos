#include "water.h"

#include <rt/atomic.h>
#include <rt/sem.h>

struct reaction
{
    struct rt_sem h2ready, hdone;
    rt_atomic_int h;
};

static struct reaction rxn = {
    .h2ready = RT_SEM_INIT(rxn.h2ready, 0),
    .hdone = RT_SEM_INIT(rxn.hdone, 0),
    .h = 0,
};

void hydrogen(void)
{
    int oldh = rt_atomic_fetch_add_explicit(&rxn.h, 1, memory_order_relaxed);
    if ((oldh % 2) == 1)
    {
        rt_sem_post(&rxn.h2ready);
    }
    rt_sem_wait(&rxn.hdone);
}

void oxygen(void)
{
    rt_sem_wait(&rxn.h2ready);
    make_water();
    rt_sem_post(&rxn.hdone);
    rt_sem_post(&rxn.hdone);
}
