#ifndef RT_SYSCALL_H
#define RT_SYSCALL_H

struct rt_task;

enum rt_syscall
{
    RT_SYSCALL_NONE,

    /* Invokes a reschedule. */
    RT_SYSCALL_SCHED,

    /* Processes a tick. */
    RT_SYSCALL_TICK,

    /* syscalls invoked on tasks */
    RT_SYSCALL_YIELD,
    RT_SYSCALL_EXIT,
    RT_SYSCALL_SLEEP,
    RT_SYSCALL_SLEEP_PERIODIC,
    RT_SYSCALL_SEM_WAIT,
    RT_SYSCALL_MUTEX_LOCK,

    /* syscalls invoked on other objects */
    RT_SYSCALL_SEM_POST,
    RT_SYSCALL_MUTEX_UNLOCK,
};

union rt_syscall_args
{
    unsigned long sleep_ticks;
    struct
    {
        unsigned long last_wake_tick;
        unsigned long period;
    } sleep_periodic;
    struct rt_sem *sem;
    struct rt_mutex *mutex;
};

struct rt_syscall_record
{
    struct rt_syscall_record *next;
    struct rt_task *task;
    union rt_syscall_args args;
    enum rt_syscall syscall;
};

void rt_syscall(struct rt_syscall_record *syscall);

/*
 * Architecture-dependent handler for syscalls. This will call rt_syscall_run
 * and perform a context switch if necessary.
 */
void rt_syscall_handler(void);

/*
 * Trigger the syscall handler.
 */
void rt_syscall_post(void);

/*
 * Perform all pending system calls and return a new context to execute or NULL
 * if no context switch is required.
 */
void *rt_syscall_run(void);

#endif /* RT_SYSCALL_H */
