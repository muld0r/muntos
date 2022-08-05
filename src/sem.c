#include <rt/sem.h>

void rt_sem_init(rt_sem_t *sem, size_t count)
{
    const struct rt_queue_config sem_cfg = {
        .buf = NULL,
        .elem_size = 1,
        .num_elems = count,
    };
    rt_queue_init(sem, &sem_cfg);
}

void rt_sem_post(rt_sem_t *sem)
{
    (void)rt_queue_send(sem, NULL);
}

void rt_sem_wait(rt_sem_t *sem)
{
    rt_queue_recv(sem, NULL);
}
