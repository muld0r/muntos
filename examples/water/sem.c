#include "water.h"

#include <rt/mutex.h>
#include <rt/sem.h>

struct reaction
{
    struct rt_sem hready, hdone;
    struct rt_mutex m;
};

void reaction_init(struct reaction *rxn)
{
    rt_sem_init(&rxn->hready, 0);
    rt_sem_init(&rxn->hdone, 0);
    rt_mutex_init(&rxn->m);
}

void hydrogen(struct reaction *rxn)
{
    rt_sem_post(&rxn->hready);
    rt_sem_wait(&rxn->hdone);
}

void oxygen(struct reaction *rxn)
{
    rt_mutex_lock(&rxn->m);
    rt_sem_wait(&rxn->hready);
    rt_sem_wait(&rxn->hready);
    rt_mutex_unlock(&rxn->m);
    make_water();
    rt_sem_post(&rxn->hdone);
    rt_sem_post(&rxn->hdone);
}
