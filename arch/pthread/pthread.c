#include <rt/context.h>
#include <rt/rt.h>

#include <rt/log.h>
#include <rt/syscall.h>
#include <rt/task.h>
#include <rt/tick.h>

#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/time.h>

#include <stdarg.h>
#include <stdio.h>

#define SIGTICK SIGALRM
#define SIGSYSCALL SIGUSR1
#define SIGRESUME SIGUSR2

#ifndef RT_LOG_ENABLE
#define RT_LOG_ENABLE 0
#endif

struct pthread_arg
{
    void (*fn)(void *);
    void *arg;
};

static pthread_t main_thread;

static void block_all_signals(sigset_t *old_sigset)
{
    /* SIGINT must always be unblocked for debugging and ctrl-C. */
    sigset_t blocked_sigset;
    sigfillset(&blocked_sigset);
    sigdelset(&blocked_sigset, SIGINT);
    pthread_sigmask(SIG_BLOCK, &blocked_sigset, old_sigset);
}

static void unblock_all_signals(void)
{
    sigset_t full_sigset;
    sigfillset(&full_sigset);
    pthread_sigmask(SIG_UNBLOCK, &full_sigset, NULL);
}

void rt_logf(const char *format, ...)
{
#if RT_LOG_ENABLE
    va_list vlist;
    va_start(vlist, format);
    sigset_t old_sigset;
    block_all_signals(&old_sigset);
    vfprintf(stderr, format, vlist);
    fflush(stderr);
    pthread_sigmask(SIG_SETMASK, &old_sigset, NULL);
#else
    (void)format;
#endif
}

static void *pthread_fn(void *arg)
{
    struct pthread_arg *parg = arg;
    void (*fn)(void *) = parg->fn;
    arg = parg->arg;
    free(parg);
    int sig;
    sigset_t resume_sigset;
    sigemptyset(&resume_sigset);
    sigaddset(&resume_sigset, SIGRESUME);
    sigwait(&resume_sigset, &sig);
    unblock_all_signals();
    fn(arg);
    rt_task_exit();
    return NULL;
}

void *rt_context_create(void (*fn)(void *), void *arg, void *stack,
                        size_t stack_size)
{
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstack(&attr, stack, stack_size);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    struct pthread_arg *parg = malloc(sizeof *parg);
    parg->fn = fn;
    parg->arg = arg;

    /*
     * Launch each thread with interrupts disabled so only the active thread
     * will receive interrupts.
     */
    sigset_t old_sigset;
    block_all_signals(&old_sigset);

    pthread_t thread;
    pthread_create(&thread, &attr, pthread_fn, parg);

    pthread_sigmask(SIG_SETMASK, &old_sigset, NULL);

    pthread_attr_destroy(&attr);

    return (void *)thread;
}

void rt_syscall_post(void)
{
    pthread_kill(pthread_self(), SIGSYSCALL);
}

__attribute__((noreturn)) static void resume_handler(int sig)
{
    (void)sig;
    /* This is always an error, so don't use rt_logf. */
    fprintf(stderr, "thread %lx received a resume but was not suspended\n",
            (unsigned long)pthread_self());
    fflush(stderr);
    exit(1);
}

static void syscall_handler(int sig)
{
    (void)sig;
    rt_syscall_handler();
}

void rt_syscall_handler(void)
{
    void *newctx = rt_syscall_run();

    if (newctx)
    {
        /* Block signals on the suspending thread. */
        block_all_signals(NULL);

        pthread_kill((pthread_t)newctx, SIGRESUME);

        *rt_prev_context = (void *)pthread_self();
        sigset_t resume_sigset;
        sigemptyset(&resume_sigset);
        sigaddset(&resume_sigset, SIGRESUME);
        int sig;
        sigwait(&resume_sigset, &sig);
    }
}

static void tick_handler(int sig)
{
    (void)sig;
    rt_tick_advance();
}

__attribute__((noreturn)) static void idle_fn(void *arg)
{
    (void)arg;
    sigset_t sigset;
    sigfillset(&sigset);
    sigdelset(&sigset, SIGINT);
    for (;;)
    {
        /* Block signals and wait for one to occur. */
        block_all_signals(NULL);
        rt_logf("%s waiting for signal\n", rt_task_name());
        int sig;
        sigwait(&sigset, &sig);

        /* After receiving a signal, re-trigger it and unblock signals. */
        pthread_kill(pthread_self(), sig);
        unblock_all_signals();
    }
}

void rt_start(void)
{
    block_all_signals(NULL);

    static char idle_stack[PTHREAD_STACK_MIN];
    pthread_t idle_thread =
        (pthread_t)rt_context_create(idle_fn, NULL, idle_stack,
                                     sizeof idle_stack);

    /* The tick handler must block SIGSYSCALL. */
    struct sigaction tick_action = {
        .sa_handler = tick_handler,
    };
    sigemptyset(&tick_action.sa_mask);
    sigaddset(&tick_action.sa_mask, SIGSYSCALL);
    sigaction(SIGTICK, &tick_action, NULL);

    /* The syscall handler doesn't need to block any signals.
     * Each signal handler blocks itself implicitly. */
    struct sigaction syscall_action = {
        .sa_handler = syscall_handler,
    };
    sigemptyset(&syscall_action.sa_mask);
    sigaction(SIGSYSCALL, &syscall_action, NULL);

    /* The handler for SIGRESUME is just to catch spurious resumes and error
     * out. Threads expecting a SIGRESUME must block it and sigwait on it. This
     * should be treated as fatal, so block all signals other than SIGINT. */
    struct sigaction resume_action = {
        .sa_handler = resume_handler,
    };
    sigfillset(&resume_action.sa_mask);
    sigdelset(&resume_action.sa_mask, SIGINT);
    sigaction(SIGRESUME, &resume_action, NULL);

    static const struct timeval milli = {
        .tv_sec = 0,
        .tv_usec = 1000,
    };
    struct itimerval timer = {
        .it_interval = milli,
        .it_value = milli,
    };
    setitimer(ITIMER_REAL, &timer, NULL);

    main_thread = pthread_self();

    pthread_kill(idle_thread, SIGRESUME);
    pthread_kill(idle_thread, SIGSYSCALL);

    /* Sending a SIGRESUME to the main thread stops the scheduler. */
    sigset_t resume_sigset;
    sigemptyset(&resume_sigset);
    sigaddset(&resume_sigset, SIGRESUME);

    int sig;
    sigwait(&resume_sigset, &sig);

    /* Prevent new SIGTICKs */
    static const struct timeval zero = {
        .tv_sec = 0,
        .tv_usec = 0,
    };
    timer.it_interval = zero;
    timer.it_value = zero;
    setitimer(ITIMER_REAL, &timer, NULL);

    /* Change handler to SIG_IGN to drop any pending signals. */
    struct sigaction action = {.sa_handler = SIG_IGN};
    sigemptyset(&action.sa_mask);

    sigaction(SIGTICK, &action, NULL);
    sigaction(SIGRESUME, &action, NULL);
    sigaction(SIGSYSCALL, &action, NULL);

    unblock_all_signals();

    /* Restore the default handlers. */
    action.sa_handler = SIG_DFL;
    sigaction(SIGTICK, &action, NULL);
    sigaction(SIGRESUME, &action, NULL);
    sigaction(SIGSYSCALL, &action, NULL);
}

void rt_stop(void)
{
    block_all_signals(NULL);
    pthread_kill(main_thread, SIGRESUME);
}
