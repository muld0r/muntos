#include <rt/mutex.h>
#include <rt/rt.h>
#include <rt/task.h>

static const int n = 50;

static void increment(void *arg)
{
    static RT_MUTEX(mutex);
    int *x = arg;

    for (int i = 0; i < n; ++i)
    {
        rt_mutex_lock(&mutex);
        ++*x;
        rt_mutex_unlock(&mutex);
    }

    /* Only the second task to finish will call rt_stop. */
    static RT_SEM(stop_sem, 1);
    if (!rt_sem_trywait(&stop_sem))
    {
        rt_stop();
    }
}

int main(void)
{
    static int x;
    __attribute__((aligned(STACK_ALIGN))) static char stack0[TASK_STACK_SIZE],
        stack1[TASK_STACK_SIZE];
    RT_TASK(increment, &x, stack0, 1);
    RT_TASK(increment, &x, stack1, 1);
    rt_start();

    if (x != n * 2)
    {
        return 1;
    }
}
