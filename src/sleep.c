#include <rt/sleep.h>

#include <rt/syscall.h>
#include <rt/log.h>

void rt_sleep(unsigned long ticks)
{
    struct rt_syscall_record sleep_record;
    sleep_record.syscall = RT_SYSCALL_SLEEP;
    sleep_record.args.sleep.task = rt_task_self();
    sleep_record.args.sleep.ticks = ticks;
    rt_logf("syscall: %s sleep %lu\n", rt_task_name(), ticks);
    rt_syscall(&sleep_record);
}

void rt_sleep_periodic(unsigned long *last_wake_tick, unsigned long period)
{
    struct rt_syscall_record sleep_record;
    sleep_record.syscall = RT_SYSCALL_SLEEP_PERIODIC;
    sleep_record.args.sleep_periodic.task = rt_task_self();
    sleep_record.args.sleep_periodic.last_wake_tick = *last_wake_tick;
    sleep_record.args.sleep_periodic.period = period;
    rt_logf("syscall: %s sleep periodic, last wake = %lu, period = %lu\n",
            rt_task_name(), *last_wake_tick, period);
    *last_wake_tick += period;
    rt_syscall(&sleep_record);
}
