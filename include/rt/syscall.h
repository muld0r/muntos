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

    /* Add a task to the ready list. */
    RT_SYSCALL_TASK_READY,
};

union rt_syscall_args
{
    struct
    {
        unsigned long ticks;
    } sleep;
    struct
    {
        unsigned long last_wake_tick;
        unsigned long period;
    } sleep_periodic;
    struct
    {
        struct rt_sem *sem;
    } sem_wait;
    struct
    {
        struct rt_sem *sem;
        unsigned long ticks;
    } sem_timedwait;
    struct
    {
        struct rt_sem *sem;
        int increment;
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
 * and perform a context switch if necessary. The syscall interrupt must be
 * masked and cleared before calling rt_syscall_run.
 */
void rt_syscall_handler(void);

/*
 * Architecture-dependent trigger for the syscall handler. This should cause
 * the syscall handler to run before this function returns if called from a
 * task. If called from an interrupt, the syscall should run once no other
 * interrupts are running, but before another task gets to run.
 */
void rt_syscall_pend(void);

/*
 * Perform all pending system calls and return a new context to execute or NULL
 * if no context switch is required.
 */
void *rt_syscall_run(void);

#endif /* RT_SYSCALL_H */
