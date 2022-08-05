#ifndef RT_SYSCALL_H
#define RT_SYSCALL_H

enum rt_syscall
{
    RT_SYSCALL_NOP,
    RT_SYSCALL_YIELD,
};

/*
 * Invoke a syscall.
 */
void rt_syscall(enum rt_syscall syscall);

/*
 * Perform the syscall. Should be called by the syscall handler.
 */
void rt_syscall_run(enum rt_syscall syscall);

#endif // RT_SYSCALL_H
