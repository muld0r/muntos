#include <rt/mutex.h>
#include <rt/sleep.h>
#include <rt/rt.h>

static const int n = 50;
static RT_MUTEX(mutex);

static int x;

static void increment(void *arg)
{
    (void)arg;
    for (int i = 0; i < n; ++i)
    {
        rt_mutex_lock(&mutex);
        ++x;
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
    __attribute__((aligned(STACK_ALIGN))) static char stack0[TASK_STACK_SIZE],
        stack1[TASK_STACK_SIZE];
    RT_TASK(increment, NULL, stack0, 1);
    RT_TASK(increment, NULL, stack1, 1);
    rt_start();

    if (x != n * 2)
    {
        return 1;
    }
}
