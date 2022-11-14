#ifndef RT_SYSCALL_H
#define RT_SYSCALL_H

#include <rt/task.h>

#include <stdbool.h>

enum rt_syscall
{
    RT_SYSCALL_NONE,

    /* Invokes a reschedule. */
    RT_SYSCALL_SCHED,

    /* Processes a tick. */
    RT_SYSCALL_TICK,

    /* Yield from a task. */
    RT_SYSCALL_YIELD,

    /* Exit from a task. */
    RT_SYSCALL_EXIT,

    /* Sleep from a task. */
    RT_SYSCALL_SLEEP,
    RT_SYSCALL_SLEEP_PERIODIC,

    /* Wait on a semaphore from a task. */
    RT_SYSCALL_SEM_WAIT,

    /* Lock a mutex from a task. */
    RT_SYSCALL_MUTEX_LOCK,

    /* Post a semaphore from a task or interrupt. */
    RT_SYSCALL_SEM_POST,

    /* Unlock a mutex from a task or interrupt. */
    RT_SYSCALL_MUTEX_UNLOCK,

    /* Send or receive to a queue from a task. */
    RT_SYSCALL_QUEUE_SEND,
    RT_SYSCALL_QUEUE_RECV,

    /* Wake up tasks that were blocked on queue operations. */
    RT_SYSCALL_QUEUE_WAKE,
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
    struct rt_queue *queue;
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
