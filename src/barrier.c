#include <muntos/barrier.h>

#include <muntos/log.h>
#include <muntos/task.h>

void rt_barrier_init(struct rt_barrier *barrier, int count)
{
    barrier->tasks = 0;
    barrier->count = count;
    rt_sem_init(&barrier->sem, count);
    rt_mutex_init(&barrier->mutex);
    rt_cond_init(&barrier->cond);
}

bool rt_barrier_wait(struct rt_barrier *barrier)
{
    rt_sem_wait(&barrier->sem);

    rt_mutex_lock(&barrier->mutex);
    ++barrier->tasks;
    if (barrier->tasks == barrier->count)
    {
        rt_cond_broadcast(&barrier->cond);
    }
    else
    {
        rt_cond_wait(&barrier->cond, &barrier->mutex);
    }
    --barrier->tasks;
    const bool complete = barrier->tasks == 0;
    if (complete)
    {
        rt_sem_post_n(&barrier->sem, barrier->count);
    }
    rt_mutex_unlock(&barrier->mutex);

    return complete;
}
