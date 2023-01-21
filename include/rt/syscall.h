#ifndef RT_SYSCALL_H
#define RT_SYSCALL_H

struct rt_task;

enum rt_syscall
{
    /* Processes a tick. */
    RT_SYSCALL_TICK,

    /* Exit from a task. */
    RT_SYSCALL_EXIT,

    /* Sleep from a task. */
    RT_SYSCALL_SLEEP,
    RT_SYSCALL_SLEEP_PERIODIC,

    /* Wait on a semaphore from a task. */
    RT_SYSCALL_SEM_WAIT,
    RT_SYSCALL_SEM_TIMEDWAIT,

    /* Post a semaphore from a task or interrupt. */
    RT_SYSCALL_SEM_POST,
};

union rt_syscall_args
{
    struct
    {
        struct rt_task *task;
    } exit;
    struct
    {
        struct rt_task *task;
        unsigned long ticks;
    } sleep;
    struct
    {
        struct rt_task *task;
        unsigned long last_wake_tick;
        unsigned long period;
    } sleep_periodic;
    struct
    {
        struct rt_task *task;
        struct rt_sem *sem;
    } sem_wait;
    struct
    {
        struct rt_task *task;
        struct rt_sem *sem;
        unsigned long ticks;
    } sem_timedwait;
    struct
    {
        struct rt_sem *sem;
    } sem_post;
};

struct rt_syscall_record
{
    struct rt_syscall_record *next;
    union rt_syscall_args args;
    enum rt_syscall syscall;
};

void rt_syscall(struct rt_syscall_record *record);

/*
 * Architecture-dependent handler for syscalls. This will call rt_syscall_run
 * and perform a context switch if necessary.
 */
void rt_syscall_handler(void);

/*
 * Architecture-dependent trigger for the syscall handler. This should cause
 * the syscall handler to run before this function returns if called from a
 * task, or when no other interrupts are running, but before another task gets
 * to run, if called from an interrupt.
 */
void rt_syscall_pend(void);

/*
 * Perform all pending system calls and return a new context to execute or NULL
 * if no context switch is required.
 */
void *rt_syscall_run(void);

#endif /* RT_SYSCALL_H */
