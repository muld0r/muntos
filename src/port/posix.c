#include <rt/context.h>
#include <rt/port.h>

#include <rt/critical.h>
#include <rt/syscall.h>
#include <rt/rt.h>

#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/time.h>

#include <stdio.h>

#define SIGSUSPEND SIGUSR1
#define SIGRESUME SIGUSR2
#define SIGSYSCALL SIGVTALRM

static sigset_t empty_sigset, resume_sigset, blocked_sigset;

struct rt_context
{
    pthread_t thread;
};

struct pthread_arg
{
    void (*fn)(void);
};

void rt_disable_interrupts(void)
{
    pthread_sigmask(SIG_SETMASK, &blocked_sigset, NULL);
}

void rt_enable_interrupts(void)
{
    // SIGRESUME stays blocked
    pthread_sigmask(SIG_SETMASK, &resume_sigset, NULL);
}

static void suspend_handler(int sig)
{
#ifdef RT_LOG
    printf("thread id %p suspended\n", (void *)pthread_self());
    fflush(stdout);
#endif
    sigwait(&resume_sigset, &sig);
}

static void setup_sigsets(void)
{
    // SIGINT must always be unblocked for debugging and ctrl-C.
    sigfillset(&blocked_sigset);
    sigdelset(&blocked_sigset, SIGINT);

    // SIGRESUME stays blocked so that we can always sigwait on it.
    sigemptyset(&resume_sigset);
    sigaddset(&resume_sigset, SIGRESUME);

    sigemptyset(&empty_sigset);
}

static void *pthread_fn(void *arg)
{
    struct pthread_arg *parg = arg;
    void (*fn)(void) = parg->fn;
    free(parg);
    int sig;
    sigwait(&resume_sigset, &sig);
    rt_enable_interrupts();
    fn();
    return NULL;
}

struct rt_context *rt_context_create(void *stack, size_t stack_size,
                                       void (*fn)(void))
{
    static pthread_once_t sigset_setup_once = PTHREAD_ONCE_INIT;
    pthread_once(&sigset_setup_once, setup_sigsets);

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstack(&attr, stack, stack_size);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    struct pthread_arg *parg = malloc(sizeof *parg);
    struct rt_context *ctx = malloc(sizeof *ctx);
    parg->fn = fn;

    // launch each thread with all signals blocked
    sigset_t old_sigset;
    pthread_sigmask(SIG_SETMASK, &blocked_sigset, &old_sigset);
    pthread_create(&ctx->thread, &attr, pthread_fn, parg);

    // restore the original mask
    pthread_sigmask(SIG_SETMASK, &old_sigset, NULL);

    pthread_attr_destroy(&attr);

    return ctx;
}

void rt_context_save(struct rt_context *ctx)
{
#ifdef RT_LOG
    printf("delivering SIGSUSPEND to %p\n", (void *)ctx->thread);
    fflush(stdout);
#endif
    pthread_kill(ctx->thread, SIGSUSPEND);
}

void rt_context_load(struct rt_context *ctx)
{
#ifdef RT_LOG
    printf("delivering SIGRESUME to %p\n", (void *)ctx->thread);
    fflush(stdout);
#endif
    pthread_kill(ctx->thread, SIGRESUME);
}

void rt_context_destroy(struct rt_context *ctx)
{
    free(ctx);
}

static volatile sig_atomic_t pending_syscall = 0;

void rt_syscall(enum rt_syscall syscall)
{
    pending_syscall = (sig_atomic_t)syscall;
    pthread_kill(pthread_self(), SIGSYSCALL);
}

static void syscall_handler(int sig)
{
#ifdef RT_LOG
    printf("syscall_handler %d\n", pending_syscall);
    fflush(stdout);
#endif
    (void)sig;
    switch ((enum rt_syscall)pending_syscall)
    {
    case RT_SYSCALL_NOP:
        break;
    case RT_SYSCALL_YIELD:
        rt_sched();
        break;
    }
    pending_syscall = 0;
}

static void tick_handler(int sig)
{
    (void)sig;
    rt_tick();
}

// list of signals in increasing priority order
// each signal handler masks all signals before it in the list when active
static struct
{
    void (*sigfn)(int);
    int sig;
} signal_table[] = {
    {.sigfn = suspend_handler, .sig = SIGSUSPEND},
    {.sigfn = syscall_handler, .sig = SIGSYSCALL},
    {.sigfn = tick_handler, .sig = SIGALRM},
    {.sigfn = NULL, .sig = 0},
};

static pthread_t main_thread;

void rt_port_start(void)
{
    rt_disable_interrupts();

    struct sigaction action = {.sa_handler = NULL, .sa_mask = empty_sigset};

    for (size_t i = 0; signal_table[i].sig != 0; ++i)
    {
        action.sa_handler = signal_table[i].sigfn;
        sigaction(signal_table[i].sig, &action, NULL);
        sigaddset(&action.sa_mask, signal_table[i].sig);
    }

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

    rt_sched();

    int sig;
    sigset_t stop_sigset = resume_sigset;
    // Allow sigint to stop the scheduler.
    sigaddset(&stop_sigset, SIGINT);
    pthread_sigmask(SIG_BLOCK, &stop_sigset, NULL);
    sigwait(&stop_sigset, &sig);

    rt_disable_interrupts();

    // prevent new SIGALRMs
    static const struct timeval zero = {
        .tv_sec = 0,
        .tv_usec = 0,
    };
    timer.it_interval = zero;
    timer.it_value = zero;
    setitimer(ITIMER_REAL, &timer, NULL);

    // change handler to SIG_IGN to drop any pending signals
    action.sa_handler = SIG_IGN;
    action.sa_mask = empty_sigset;

    for (size_t i = 0; signal_table[i].sig != 0; ++i)
    {
        sigaction(signal_table[i].sig, &action, NULL);
    }

    // Re-enable all signals.
    pthread_sigmask(SIG_SETMASK, &empty_sigset, NULL);

    // Restore the default handlers.
    action.sa_handler = SIG_DFL;
    for (size_t i = 0; signal_table[i].sig != 0; ++i)
    {
        sigaction(signal_table[i].sig, &action, NULL);
    }
}

void rt_port_stop(void)
{
    pthread_kill(main_thread, SIGRESUME);
}
