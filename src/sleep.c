#include <rt/sleep.h>

#include <rt/syscall.h>
#include <rt/task.h>
#include <rt/rt.h>

void rt_sleep(unsigned long ticks)
{
    struct rt_task *self = rt_self();
    self->syscall_args.sleep_ticks = ticks;
    self->syscall_record.syscall = RT_SYSCALL_SLEEP;
    rt_syscall_push(&self->syscall_record);
    rt_syscall_post();
}

void rt_sleep_periodic(unsigned long *last_wake_tick, unsigned long period)
{
    struct rt_task *self = rt_self();
    self->syscall_args.sleep_periodic.last_wake_tick = *last_wake_tick;
    *last_wake_tick += period;
    self->syscall_args.sleep_periodic.period = period;
    self->syscall_record.syscall = RT_SYSCALL_SLEEP_PERIODIC;
    rt_syscall_push(&self->syscall_record);
    rt_syscall_post();
}
