#include <rt/sem.h>

#include <rt/critical.h>

void rt_sem_init(struct rt_sem *sem, unsigned int initial_value)
{
    rt_list_init(&sem->wait_list);
    sem->value = initial_value;
    sem->max_value = UINT_MAX;
}

void rt_sem_binary_init(struct rt_sem *sem, unsigned int initial_value)
{
    rt_list_init(&sem->wait_list);
    sem->value = initial_value;
    sem->max_value = 1;
}

void rt_sem_post(struct rt_sem *sem)
{
    // TODO: don't use a critical section to protect the semaphore
    rt_critical_begin();
    if (sem->value < sem->max_value)
    {
        ++sem->value;
    }
    if (!rt_list_is_empty(&sem->wait_list))
    {
        struct rt_list *const node = rt_list_pop_front(&sem->wait_list);
        struct rt_task *const waiting_task =
            rt_list_item(node, struct rt_task, list);
        rt_task_ready(waiting_task);
    }
    rt_critical_end();
}

void rt_sem_wait(struct rt_sem *sem)
{
    rt_critical_begin();
    if (sem->value == 0)
    {
        rt_list_push_back(&sem->wait_list, &rt_self()->list);
        rt_yield();
        rt_critical_end();
        rt_critical_begin();
    }
    --sem->value;
    rt_critical_end();
}
