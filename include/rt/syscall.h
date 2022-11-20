#ifndef RT_SYSCALL_H
#define RT_SYSCALL_H

#include <rt/task.h>

#include <stdbool.h>

enum rt_syscall
{
    /* Reschedule. */
    RT_SYSCALL_SCHED,

    /* Processes a tick. */
    RT_SYSCALL_TICK,

    /* Exit from a task. */
    RT_SYSCALL_EXIT,

    /* Sleep from a task. */
    RT_SYSCALL_SLEEP,
    RT_SYSCALL_SLEEP_PERIODIC,

    /* Wait on a semaphore from a task. */
    RT_SYSCALL_SEM_WAIT,

    /* Post a semaphore from a task or interrupt. */
    RT_SYSCALL_SEM_POST,

    /* Lock a mutex from a task. */
    RT_SYSCALL_MUTEX_LOCK,

    /* Unlock a mutex from a task or interrupt. */
    RT_SYSCALL_MUTEX_UNLOCK,
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
        struct rt_sem *sem;
    } sem_post;
    struct
    {
        struct rt_task *task;
        struct rt_mutex *mutex;
    } mutex_lock;
    struct
    {
        struct rt_mutex *mutex;
    } mutex_unlock;
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
 * Trigger the syscall handler.
 */
void rt_syscall_post(void);

/*
 * Perform all pending system calls and return a new context to execute or NULL
 * if no context switch is required.
 */
void *rt_syscall_run(void);

#endif /* RT_SYSCALL_H */
