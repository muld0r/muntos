#include <rt/log.h>
#include <rt/rt.h>
#include <rt/rwlock.h>
#include <rt/sleep.h>
#include <rt/task.h>

static RT_RWLOCK(lock);
static unsigned long x = 0;
static unsigned long y = 0;

#define NUM_READERS 4

static volatile bool mismatch = false;

static void reader(void)
{
    for (;;)
    {
        rt_rwlock_rlock(&lock);
        const unsigned long rx = x;
        const unsigned long ry = y;
        rt_rwlock_runlock(&lock);
        rt_logf("%lu %lu\n", rx, ry);
        if (rx != ry)
        {
            mismatch = true;
        }
    }
}

static void writer(void)
{
    for (;;)
    {
        rt_rwlock_wlock(&lock);
        ++x;
        ++y;
        rt_rwlock_wunlock(&lock);
        rt_sleep(1);
    }
}

static void timeout(void)
{
    rt_sleep(1000);
    rt_stop();
}

int main(void)
{
    static char reader_stacks[NUM_READERS][TASK_STACK_SIZE]
        __attribute__((aligned(STACK_ALIGN)));
    static struct rt_task reader_tasks[NUM_READERS];
    for (int i = 0; i < NUM_READERS; ++i)
    {
        rt_task_init(&reader_tasks[i], reader, "reader", 1, reader_stacks[i],
                     TASK_STACK_SIZE);
    }

    static char writer_stack[TASK_STACK_SIZE]
        __attribute__((aligned(STACK_ALIGN)));
    RT_TASK(writer, writer_stack, 1);

    static char timeout_stack[TASK_STACK_SIZE]
        __attribute__((aligned(STACK_ALIGN)));
    RT_TASK(timeout, timeout_stack, 2);

    rt_start();

    if (mismatch)
    {
        return 1;
    }
}
