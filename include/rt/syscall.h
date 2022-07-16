#ifndef RT_SYSCALL_H
#define RT_SYSCALL_H

enum rt_syscall
{
    RT_SYSCALL_NOP,
    RT_SYSCALL_YIELD,
};

void rt_syscall(enum rt_syscall syscall);

#endif // RT_SYSCALL_H
