#include <rt/context.h>
#include <rt/rt.h>

#include <rt/syscall.h>
#include <rt/tick.h>

#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/time.h>

#include <stdatomic.h>
#include <stdio.h>

#define SIGTICK SIGALRM
#define SIGSUSPEND SIGRTMIN
#define SIGRESUME (SIGRTMIN + 1)
#define SIGSYSCALL (SIGRTMIN + 2)
#define SIGRTSTOP (SIGRTMIN + 3)

#define log(...)                                                               \
    do                                                                         \
    {                                                                          \
        fprintf(stderr, __VA_ARGS__);                                          \
        fflush(stderr);                                                        \
    } while (0)

#ifndef RT_LOG_EVENTS
#define RT_LOG_EVENTS 0
#endif

#if RT_LOG_EVENTS
#define log_event(...) log(__VA_ARGS__)
#else
#define log_event(...)
#endif

struct pthread_arg
{
    void (*fn)(void);
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

static void *pthread_fn(void *arg)
{
    log_event("thread %lx created\n", (unsigned long)pthread_self());
    struct pthread_arg *parg = arg;
    void (*fn)(void) = parg->fn;
    free(parg);
    int sig;
    sigset_t resume_sigset;
    sigemptyset(&resume_sigset);
    sigaddset(&resume_sigset, SIGRESUME);
    sigwait(&resume_sigset, &sig);
    log_event("thread %lx starting\n", (unsigned long)pthread_self());
    unblock_all_signals();
    fn();
    log_event("thread %lx exiting\n", (unsigned long)pthread_self());
    rt_task_exit();
    return NULL;
}

void *rt_context_create(void *stack, size_t stack_size, void (*fn)(void))
{
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstack(&attr, stack, stack_size);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    struct pthread_arg *parg = malloc(sizeof *parg);
    parg->fn = fn;

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
    /* This is always an error, so don't use log_event. */
    log("thread %lx received a resume but was not suspended\n",
        (unsigned long)pthread_self());
    exit(1);
}

static void suspend_handler(int sig)
{
    log_event("thread %lx suspending\n", (unsigned long)pthread_self());
    sigset_t resume_sigset;
    sigemptyset(&resume_sigset);
    sigaddset(&resume_sigset, SIGRESUME);
    sigwait(&resume_sigset, &sig);
    log_event("thread %lx resuming\n", (unsigned long)pthread_self());
    unblock_all_signals();
}

static void syscall_handler(int sig)
{
    log_event("thread %lx running syscall\n", (unsigned long)pthread_self());
    (void)sig;
    struct rt_task *oldtask = rt_self();
    void *newctx = rt_syscall_run();
    if (newctx)
    {
        /* Block signals on the suspending thread. */
        sigset_t blocked_sigset;
        sigfillset(&blocked_sigset);
        sigdelset(&blocked_sigset, SIGINT);
        sigdelset(&blocked_sigset, SIGSUSPEND);
        pthread_sigmask(SIG_BLOCK, &blocked_sigset, NULL);

        oldtask->ctx = (void *)pthread_self();
        pthread_kill(pthread_self(), SIGSUSPEND);
        pthread_kill((pthread_t)newctx, SIGRESUME);
    }
}

static void tick_handler(int sig)
{
    log_event("thread %lx received a tick\n", (unsigned long)pthread_self());
    (void)sig;
    rt_tick_advance();
}

__attribute__((noreturn)) static void idle_fn(void)
{
    sigset_t sigset;
    sigfillset(&sigset);
    sigdelset(&sigset, SIGINT);
    for (;;)
    {
        /* Block signals and wait for one to occur. */
        block_all_signals(NULL);
        log_event("thread %lx waiting for signal\n",
                  (unsigned long)pthread_self());
        int sig;
        sigwait(&sigset, &sig);
        log_event("thread %lx received signal %d while waiting\n",
                  (unsigned long)pthread_self(), sig);

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
        (pthread_t)rt_context_create(idle_stack, sizeof idle_stack, idle_fn);

    /* The tick handler must block SIGSYSCALL and SIGSUSPEND. */
    struct sigaction tick_action = {
        .sa_handler = tick_handler,
    };
    sigemptyset(&tick_action.sa_mask);
    sigaddset(&tick_action.sa_mask, SIGSYSCALL);
    sigaddset(&tick_action.sa_mask, SIGSUSPEND);
    sigaction(SIGTICK, &tick_action, NULL);

    /* The handler for SIGRESUME is just to catch spurious resumes and error
     * out. Threads expecting a SIGRESUME must block it and sigwait on it. This
     * should be treated as fatal, so block all signals other than SIGINT. */
    struct sigaction resume_action = {
        .sa_handler = resume_handler,
    };
    sigfillset(&resume_action.sa_mask);
    sigdelset(&resume_action.sa_mask, SIGINT);
    sigaction(SIGRESUME, &resume_action, NULL);

    /* The suspend handler must block all signals so that other signals are
     * only delivered to the active thread. */
    struct sigaction suspend_action = {
        .sa_handler = suspend_handler,
    };
    sigfillset(&suspend_action.sa_mask);
    sigdelset(&suspend_action.sa_mask, SIGINT);
    sigaction(SIGSUSPEND, &suspend_action, NULL);

    /* The syscall handler must block SIGSUSPEND so that it can suspend the
     * current thread and then resume the next one. This way when the syscall
     * handler returns the thread will actually become suspended. */
    struct sigaction syscall_action = {
        .sa_handler = syscall_handler,
    };
    sigemptyset(&syscall_action.sa_mask);
    sigaddset(&syscall_action.sa_mask, SIGSUSPEND);
    sigaction(SIGSYSCALL, &syscall_action, NULL);

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

    /*
     * Allow SIGRTSTOP to stop the scheduler.
     */
    sigset_t stop_sigset;
    sigemptyset(&stop_sigset);
    sigaddset(&stop_sigset, SIGRTSTOP);

    int sig;
    sigwait(&stop_sigset, &sig);

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
    sigaction(SIGSUSPEND, &action, NULL);
    sigaction(SIGSYSCALL, &action, NULL);

    unblock_all_signals();

    /* Restore the default handlers. */
    action.sa_handler = SIG_DFL;
    sigaction(SIGTICK, &action, NULL);
    sigaction(SIGRESUME, &action, NULL);
    sigaction(SIGSUSPEND, &action, NULL);
    sigaction(SIGSYSCALL, &action, NULL);
}

void rt_stop(void)
{
    block_all_signals(NULL);
    pthread_kill(main_thread, SIGRTSTOP);
}
