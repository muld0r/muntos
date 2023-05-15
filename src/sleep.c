#include <muntos/sleep.h>

#include <muntos/log.h>
#include <muntos/syscall.h>
#include <muntos/task.h>

void rt_sleep(unsigned long ticks)
{
    if (ticks == 0)
    {
        return;
    }

    struct rt_syscall_record *const sleep_record = &rt_task_self()->record;
    sleep_record->syscall = RT_SYSCALL_SLEEP;
    sleep_record->args.sleep.ticks = ticks;
    rt_logf("syscall: %s sleep %lu\n", rt_task_name(), ticks);
    rt_syscall(sleep_record);
}

void rt_sleep_periodic(unsigned long *last_wake_tick, unsigned long period)
{
    if (period == 0)
    {
        return;
    }

    struct rt_syscall_record *const sleep_record = &rt_task_self()->record;
    sleep_record->syscall = RT_SYSCALL_SLEEP_PERIODIC;
    sleep_record->args.sleep_periodic.last_wake_tick = *last_wake_tick;
    sleep_record->args.sleep_periodic.period = period;
    rt_logf("syscall: %s sleep periodic, last wake = %lu, period = %lu\n",
            rt_task_name(), *last_wake_tick, period);
    *last_wake_tick += period;
    rt_syscall(sleep_record);
}
