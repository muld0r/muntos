#include "water.h"

#include <muntos/cond.h>
#include <muntos/mutex.h>

struct reaction
{
    int h;
    struct rt_cond hready, hdone;
    struct rt_mutex m;
};

static struct reaction rxn = {
    .h = 0,
    .hready = RT_COND_INIT(rxn.hready),
    .hdone = RT_COND_INIT(rxn.hdone),
    .m = RT_MUTEX_INIT(rxn.m),
};

void hydrogen(void)
{
    rt_mutex_lock(&rxn.m);
    ++rxn.h;
    rt_cond_signal(&rxn.hready);
    rt_cond_wait(&rxn.hdone, &rxn.m);
    rt_mutex_unlock(&rxn.m);
}

void oxygen(void)
{
    rt_mutex_lock(&rxn.m);
    while (rxn.h < 2)
    {
        rt_cond_wait(&rxn.hready, &rxn.m);
    }
    make_water();
    rxn.h -= 2;
    rt_mutex_unlock(&rxn.m);
    rt_cond_signal(&rxn.hdone);
    rt_cond_signal(&rxn.hdone);
}
