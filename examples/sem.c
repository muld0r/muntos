#include <rt/rt.h>
#include <rt/sem.h>

static const int n = 10;

static void poster(void *arg)
{
    struct rt_sem *sem = arg;
    for (int i = 1; i <= n; ++i)
    {
        rt_sem_post(sem);
    }
}

static void waiter(void *arg)
{
    struct rt_sem *sem = arg;
    for (int i = 1; i <= n; ++i)
    {
        rt_sem_wait(sem);
    }
    rt_stop();
}

int main(void)
{
    static RT_SEM(sem, 0);
    __attribute__((aligned(STACK_ALIGN))) static char
        poster_stack[TASK_STACK_SIZE],
        waiter_stack[TASK_STACK_SIZE];
    RT_TASK(poster, &sem, poster_stack, 1);
    RT_TASK(waiter, &sem, waiter_stack, 1);
    rt_start();
}
