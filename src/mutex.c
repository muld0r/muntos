#include <rt/mutex.h>

#include <rt/log.h>
#include <rt/task.h>

void rt_mutex_init(struct rt_mutex *mutex)
{
    rt_sem_init_binary(&mutex->sem, 1);
}

void rt_mutex_lock(struct rt_mutex *mutex)
{
    rt_logf("%s mutex lock\n", rt_task_name());
    rt_sem_wait(&mutex->sem);
}

bool rt_mutex_trylock(struct rt_mutex *mutex)
{
    return rt_sem_trywait(&mutex->sem);
}

bool rt_mutex_timedlock(struct rt_mutex *mutex, unsigned long ticks)
{
    return rt_sem_timedwait(&mutex->sem, ticks);
}

void rt_mutex_unlock(struct rt_mutex *mutex)
{
    rt_logf("%s mutex unlock\n", rt_task_name());
    rt_sem_post(&mutex->sem);
}
