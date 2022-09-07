#ifndef RT_SYSCALL_H
#define RT_SYSCALL_H

enum rt_syscall
{
    RT_SYSCALL_NONE,

    /* Invokes a reschedule. */
    RT_SYSCALL_SCHED,

    /* Processes a tick. */
    RT_SYSCALL_TICK,

    /* syscalls invoked on tasks */
    RT_SYSCALL_READY,
    RT_SYSCALL_YIELD,
    RT_SYSCALL_EXIT,
    RT_SYSCALL_SLEEP,
    RT_SYSCALL_SLEEP_PERIODIC,
    RT_SYSCALL_SEM_WAIT,

    /* syscalls invoked on other objects */
    RT_SYSCALL_SEM_POST,
};

struct rt_syscall_record
{
    struct rt_syscall_record *next;
    enum rt_syscall syscall;
};

void rt_syscall_push(struct rt_syscall_record *syscall);

/*
 * Trigger the syscall handler.
 */
void rt_syscall_post(void);

/*
 * Perform all pending system calls.
 */
void rt_syscall_handler(void);

#endif // RT_SYSCALL_H
