#ifndef RT_SYSCALL_H
#define RT_SYSCALL_H

enum rt_syscall
{
    RT_SYSCALL_YIELD,
    RT_SYSCALL_EXIT,
    RT_SYSCALL_SLEEP,
    RT_SYSCALL_SLEEP_PERIODIC,
};

/*
 * Invoke a given syscall.
 */
void rt_syscall(enum rt_syscall syscall);

/*
 * Trigger the syscall handler. (architecture-specific)
 */
void rt_syscall_post(void);

/*
 * Perform the syscall.
 */
void rt_syscall_handler(void);

#endif // RT_SYSCALL_H
