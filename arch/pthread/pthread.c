#include <muntos/context.h>
#include <muntos/cycle.h>
#include <muntos/interrupt.h>
#include <muntos/log.h>
#include <muntos/muntos.h>
#include <muntos/syscall.h>

#include <muntos/task.h>
#include <muntos/tick.h>

#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/time.h>

#include <limits.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>

#define SIGTICK SIGALRM
#define SIGSYSCALL SIGUSR1
#define SIGRESUME SIGUSR2

#ifndef RT_LOG_ENABLE
#define RT_LOG_ENABLE 0
#endif

struct pthread_arg
{
    union task_fn
    {
        void (*fn)(void);
        void (*fn_with_arg)(uintptr_t);
    } task_fn;
    uintptr_t arg;
    bool has_arg;
};

static pthread_t main_thread;
static bool rt_started = false;

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
    vprintf(format, vlist);
    fflush(stdout);
    pthread_sigmask(SIG_SETMASK, &old_sigset, NULL);
#else
    (void)format;
#endif
}

static void *pthread_fn(void *arg)
{
    struct pthread_arg *parg = arg;
    union task_fn task_fn = parg->task_fn;
    uintptr_t task_arg = parg->arg;
    bool has_arg = parg->has_arg;
    free(parg);
    int sig;
    sigset_t resume_sigset;
    sigemptyset(&resume_sigset);
    sigaddset(&resume_sigset, SIGRESUME);
    sigwait(&resume_sigset, &sig);
    unblock_all_signals();
    if (has_arg)
    {
        task_fn.fn_with_arg(task_arg);
    }
    else
    {
        task_fn.fn();
    }
    rt_task_exit();
    return NULL;
}

static void *context_create(struct pthread_arg *parg, void *stack,
                            size_t stack_size)
{
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstack(&attr, stack, stack_size);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

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

void *rt_context_create(void (*fn)(void), void *stack, size_t stack_size)
{
    struct pthread_arg *parg = malloc(sizeof *parg);
    parg->task_fn.fn = fn;
    parg->has_arg = false;
    return context_create(parg, stack, stack_size);
}

void *rt_context_create_arg(void (*fn)(uintptr_t), uintptr_t arg, void *stack,
                            size_t stack_size)
{
    struct pthread_arg *parg = malloc(sizeof *parg);
    parg->task_fn.fn_with_arg = fn;
    parg->arg = arg;
    parg->has_arg = true;
    return context_create(parg, stack, stack_size);
}

void rt_syscall_pend(void)
{
    // syscalls made before rt_start are deferred.
    if (rt_started)
    {
        pthread_kill(pthread_self(), SIGSYSCALL);
    }
}

bool rt_interrupt_is_active(void)
{
    return false;
}

__attribute__((noreturn)) static void resume_handler(int sig)
{
    (void)sig;
    // This is always an error, so don't use rt_logf.
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
        *rt_context_prev = (void *)pthread_self();

        atomic_thread_fence(memory_order_release);
        pthread_kill((pthread_t)newctx, SIGRESUME);

        sigset_t resume_sigset;
        sigemptyset(&resume_sigset);
        sigaddset(&resume_sigset, SIGRESUME);
        int sig;
        sigwait(&resume_sigset, &sig);
        unblock_all_signals();
    }
}

static void tick_handler(int sig)
{
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

    RT_STACK(idle_task_stack, RT_STACK_MIN);
    pthread_t idle_thread =
        (pthread_t)rt_context_create(idle_fn, idle_task_stack,
                                     sizeof idle_task_stack);

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
    rt_started = true;

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

void rt_task_drop_privilege(void)
{
}

void rt_task_enable_fp(void)
{
}

uint32_t rt_cycle(void)
{
#if defined(__aarch64__)
    uint64_t cycles;
    __asm__ __volatile__("mrs %0, cntvct_el0" : "=r"(cycles));
    return (uint32_t)cycles;
#elif defined(__x86_64__)
    uint32_t cycles;
    __asm__ __volatile__("rdtsc" : "=a"(cycles) : : "edx");
    return cycles;
#else
    return 0;
#endif
}
