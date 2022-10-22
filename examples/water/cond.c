#include "water.h"

#include <rt/cond.h>
#include <rt/mutex.h>

struct reaction
{
    int h;
    struct rt_cond hready, hdone;
    struct rt_mutex m;
};

void reaction_init(struct reaction *rxn)
{
    rxn->h = 0;
    rt_cond_init(&rxn->hready);
    rt_cond_init(&rxn->hdone);
    rt_mutex_init(&rxn->m);
}

void hydrogen(struct reaction *rxn)
{
    rt_mutex_lock(&rxn->m);
    ++rxn->h;
    rt_cond_signal(&rxn->hready);
    rt_cond_wait(&rxn->hdone, &rxn->m);
    rt_mutex_unlock(&rxn->m);
}

void oxygen(struct reaction *rxn)
{
    rt_mutex_lock(&rxn->m);
    while (rxn->h < 2)
    {
        rt_cond_wait(&rxn->hready, &rxn->m);
    }
    make_water();
    rxn->h -= 2;
    rt_mutex_unlock(&rxn->m);
    rt_cond_signal(&rxn->hdone);
    rt_cond_signal(&rxn->hdone);
}
