#include <rt/mutex.h>
#include <rt/sleep.h>
#include <rt/rt.h>

#include <limits.h>
#include <stdio.h>

static const int n = 50;
static RT_MUTEX(mutex);

static int x;

static void inc_fn(void)
{
    for (int i = 0; i < n; ++i)
    {
        rt_mutex_lock(&mutex);
        ++x;
        rt_mutex_unlock(&mutex);
    }

    rt_sleep(1000);
    rt_stop();
}

int main(void)
{
    static char stack0[PTHREAD_STACK_MIN], stack1[PTHREAD_STACK_MIN];
    static struct rt_task inc_task0, inc_task1;
    rt_task_init(&inc_task0, inc_fn, stack0, sizeof stack0, "inc_task0", 1);
    rt_task_init(&inc_task1, inc_fn, stack1, sizeof stack1, "inc_task1", 1);
    rt_start();

    printf("%d\n", x);
}
