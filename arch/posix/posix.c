#include <rt/context.h>
#include <rt/interrupt.h>
#include <rt/rt.h>

#include <rt/critical.h>
#include <rt/syscall.h>
#include <rt/tick.h>

#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/time.h>

#include <stdio.h>

#define SIGSUSPEND SIGUSR1
#define SIGRESUME SIGUSR2
#define SIGSYSCALL SIGVTALRM
#define SIGTICK SIGALRM

struct rt_context
{
    pthread_t thread;
};

struct pthread_arg
{
    void (*fn)(void);
};

void rt_interrupt_disable(void)
{
    // SIGINT must always be unblocked for debugging and ctrl-C.
    sigset_t blocked_sigset;
    sigfillset(&blocked_sigset);
    sigdelset(&blocked_sigset, SIGINT);
    pthread_sigmask(SIG_BLOCK, &blocked_sigset, NULL);
}

void rt_interrupt_enable(void)
{
    sigset_t full_sigset;
    sigfillset(&full_sigset);
    pthread_sigmask(SIG_UNBLOCK, &full_sigset, NULL);
}

static void *pthread_fn(void *arg)
{
    struct pthread_arg *parg = arg;
    void (*fn)(void) = parg->fn;
    free(parg);
    int sig;
    sigset_t resume_sigset;
    sigemptyset(&resume_sigset);
    sigaddset(&resume_sigset, SIGRESUME);
    sigwait(&resume_sigset, &sig);
    rt_interrupt_enable();
    fn();
    return NULL;
}

struct rt_context *rt_context_create(void *stack, size_t stack_size,
                                       void (*fn)(void))
{
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstack(&attr, stack, stack_size);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    struct pthread_arg *parg = malloc(sizeof *parg);
    struct rt_context *ctx = malloc(sizeof *ctx);
    parg->fn = fn;

    /*
     * Launch each thread with interrupts disabled so only the active thread
     * will receive interrupts.
     */
    sigset_t blocked_sigset, old_sigset;
    sigfillset(&blocked_sigset);
    sigdelset(&blocked_sigset, SIGINT);
    pthread_sigmask(SIG_BLOCK, &blocked_sigset, &old_sigset);

    pthread_create(&ctx->thread, &attr, pthread_fn, parg);

    pthread_sigmask(SIG_SETMASK, &old_sigset, NULL);

    pthread_attr_destroy(&attr);

    return ctx;
}

void rt_context_save(struct rt_context *ctx)
{
#ifdef RT_LOG
    printf("delivering SIGSUSPEND to %lu\n", (unsigned long)ctx->thread);
    fflush(stdout);
#endif
    pthread_kill(ctx->thread, SIGSUSPEND);
}

void rt_context_load(struct rt_context *ctx)
{
#ifdef RT_LOG
    printf("delivering SIGRESUME to %lu\n", (unsigned long)ctx->thread);
    fflush(stdout);
#endif
    pthread_kill(ctx->thread, SIGRESUME);
}

void rt_context_destroy(struct rt_context *ctx)
{
    pthread_cancel(ctx->thread);
    free(ctx);
}

static volatile sig_atomic_t pending_syscall = 0;

void rt_syscall(enum rt_syscall syscall)
{
    pending_syscall = (sig_atomic_t)syscall;
    pthread_kill(pthread_self(), SIGSYSCALL);
}

__attribute__((noreturn)) static void resume_handler(int sig)
{
    (void)sig;
#ifdef RT_LOG
    printf("thread id %lu received a resume but was not suspended\n",
           (unsigned long)pthread_self());
    fflush(stdout);
#endif
    exit(1);
}

static void suspend_handler(int sig)
{
#ifdef RT_LOG
    printf("thread id %lu suspended\n", (unsigned long)pthread_self());
    fflush(stdout);
#endif
    sigset_t resume_sigset;
    sigemptyset(&resume_sigset);
    sigaddset(&resume_sigset, SIGRESUME);
    sigwait(&resume_sigset, &sig);
#ifdef RT_LOG
    printf("thread id %lu resumed\n", (unsigned long)pthread_self());
    fflush(stdout);
#endif
}

static void syscall_handler(int sig)
{
    (void)sig;
    rt_syscall_run((enum rt_syscall)pending_syscall);
    pending_syscall = 0;
}

static void tick_handler(int sig)
{
    (void)sig;
    rt_tick_advance();
}

static pthread_t main_thread;

void rt_start(void)
{
    rt_interrupt_disable();

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

    /* The tick action need not block any signals. */
    struct sigaction tick_action = {
        .sa_handler = tick_handler,
    };
    sigemptyset(&tick_action.sa_mask);
    sigaction(SIGTICK, &tick_action, NULL);

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

    rt_yield();

    sigset_t stop_sigset;
    sigemptyset(&stop_sigset);
    sigaddset(&stop_sigset, SIGINT);
    sigaddset(&stop_sigset, SIGRESUME);

    // This will unblock other interrupts, so the yield takes effect here.
    pthread_sigmask(SIG_SETMASK, &stop_sigset, NULL);

    int sig;
    sigwait(&stop_sigset, &sig);

    rt_interrupt_disable();

    rt_end_all_tasks();

    // Prevent new SIGTICKs
    static const struct timeval zero = {
        .tv_sec = 0,
        .tv_usec = 0,
    };
    timer.it_interval = zero;
    timer.it_value = zero;
    setitimer(ITIMER_REAL, &timer, NULL);

    // Change handler to SIG_IGN to drop any pending signals.
    struct sigaction action = {.sa_handler = SIG_IGN};
    sigemptyset(&action.sa_mask);

    sigaction(SIGRESUME, &action, NULL);
    sigaction(SIGSUSPEND, &action, NULL);
    sigaction(SIGSYSCALL, &action, NULL);
    sigaction(SIGTICK, &action, NULL);

    // Re-enable all signals.
    rt_interrupt_enable();

    // Restore the default handlers.
    action.sa_handler = SIG_DFL;
    sigaction(SIGRESUME, &action, NULL);
    sigaction(SIGSUSPEND, &action, NULL);
    sigaction(SIGSYSCALL, &action, NULL);
    sigaction(SIGTICK, &action, NULL);
}

void rt_stop(void)
{
    pthread_kill(main_thread, SIGRESUME);
}
