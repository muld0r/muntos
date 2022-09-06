#include <rt/context.h>
#include <rt/interrupt.h>
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
#define SIGWAIT (SIGRTMIN + 3)
#define SIGRTSTOP (SIGRTMIN + 4)

#ifndef RT_LOG
#define RT_LOG 0
#endif

#if RT_LOG
#define log_event(...)                                                         \
    do                                                                         \
    {                                                                          \
        fprintf(stderr, __VA_ARGS__);                                          \
        fflush(stderr);                                                        \
    } while (0)
#else
#define log_event(...)
#endif

struct rt_context
{
    pthread_t thread;
};

struct pthread_arg
{
    void (*fn)(void);
};

static pthread_t main_thread;

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

static void wait_handler(int sig)
{
    /*
     * Used to temporarily allow the main thread to handle interrupts because
     * all other threads are suspended.
     */
    log_event("thread %lx waiting for signal\n", (unsigned long)pthread_self());
    sigset_t wait_sigset;
    sigfillset(&wait_sigset);
    /* Don't consume SIGINT or SIGRTSTOP here, as those should be received in
     * the main thread to allow it to clean up. */
    sigdelset(&wait_sigset, SIGINT);
    sigdelset(&wait_sigset, SIGRTSTOP);
    sigwait(&wait_sigset, &sig);
    log_event("thread %lx received signal %d while waiting\n",
              (unsigned long)pthread_self(), sig);
    /*
     * After receiving an interrupt via sigwait, re-trigger it, temporarily
     * unblock that signal, then restore the main thread's signal mask so the
     * main thread can handle this interrupt. Block SIGWAIT at the same time so
     * that wait_handler can't run again until the new signal handler returns.
     */
    sigemptyset(&wait_sigset);
    sigaddset(&wait_sigset, SIGWAIT);
    pthread_kill(pthread_self(), sig);
    sigset_t old_sigset;
    pthread_sigmask(SIG_SETMASK, &wait_sigset, &old_sigset);
    pthread_sigmask(SIG_SETMASK, &old_sigset, NULL);
}

void rt_interrupt_wait(void)
{
    /*
     * This is called in the syscall handler when there's no active thread to
     * schedule.
     */
    pthread_kill(main_thread, SIGWAIT);
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
    rt_interrupt_enable();
    fn();
    log_event("thread %lx exiting\n", (unsigned long)pthread_self());
    rt_exit();
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
    /*
     * Prevent the thread thread that's being suspended from receiving further
     * interrupts, even before it has taken its suspend handler. The suspend
     * handler also masks these, but during a context switch, momentarily two
     * different threads could have signals unmasked unless we mask the
     * suspending thread here first. Otherwise, two threads couldpotentially
     * both run the same signal handler, and the signal handlers are not
     * written to be re-entrant.
     */
    sigset_t blocked_sigset;
    sigfillset(&blocked_sigset);
    sigdelset(&blocked_sigset, SIGINT);
    sigdelset(&blocked_sigset, SIGSUSPEND);
    pthread_sigmask(SIG_BLOCK, &blocked_sigset, NULL);
    pthread_kill(ctx->thread, SIGSUSPEND);
}

void rt_context_load(struct rt_context *ctx)
{
    pthread_kill(ctx->thread, SIGRESUME);
}

void rt_context_destroy(struct rt_context *ctx)
{
    pthread_cancel(ctx->thread);
    free(ctx);
}

void rt_syscall_post(void)
{
    atomic_signal_fence(memory_order_release);
    pthread_kill(pthread_self(), SIGSYSCALL);
}

__attribute__((noreturn)) static void resume_handler(int sig)
{
    (void)sig;
    log_event("thread %lx received a resume but was not suspended\n",
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
    rt_interrupt_enable();
}

static void syscall_handler(int sig)
{
    log_event("thread %lx running syscall\n", (unsigned long)pthread_self());
    (void)sig;
    atomic_signal_fence(memory_order_acquire);
    rt_syscall_handler();
}

static void tick_handler(int sig)
{
    log_event("thread %lx received a tick\n", (unsigned long)pthread_self());
    (void)sig;
    rt_tick_advance();
}

void rt_start(void)
{
    rt_interrupt_disable();

    /* The tick handler must block SIGSYSCALL and SIGSUSPEND. */
    struct sigaction tick_action = {
        .sa_handler = tick_handler,
    };
    sigemptyset(&tick_action.sa_mask);
    sigaddset(&tick_action.sa_mask, SIGSYSCALL);
    sigaddset(&tick_action.sa_mask, SIGSUSPEND);
    sigaddset(&tick_action.sa_mask, SIGWAIT);
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
    sigaddset(&syscall_action.sa_mask, SIGWAIT);
    sigaction(SIGSYSCALL, &syscall_action, NULL);

    /* The wait handler signal mask must not block signals because it needs to
     * temporarily unmask whatever signal it receives. The wait handler will
     * re-block signals itself before returning. */
    struct sigaction wait_action = {
        .sa_handler = wait_handler,
    };
    sigemptyset(&wait_action.sa_mask);
    sigaction(SIGWAIT, &wait_action, NULL);

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

    // Unblock syscalls so the yield can run here.
    sigset_t syscall_sigset;
    sigemptyset(&syscall_sigset);
    sigaddset(&syscall_sigset, SIGSYSCALL);
    pthread_sigmask(SIG_UNBLOCK, &syscall_sigset, NULL);
    pthread_sigmask(SIG_BLOCK, &syscall_sigset, NULL);

    // Unblock the wait signal to the main thread can wait for interrupts
    // when all other tasks become idle.
    sigset_t wait_sigset;
    sigemptyset(&wait_sigset);
    sigaddset(&wait_sigset, SIGWAIT);
    pthread_sigmask(SIG_UNBLOCK, &wait_sigset, NULL);

    // Allow SIGINT or SIGRTSTOP to stop the scheduler.
    // TODO: do we need to wait on SIGINT here for ctrl-C to work?
    // It should be always unblocked.
    sigset_t stop_sigset;
    sigemptyset(&stop_sigset);
    sigaddset(&stop_sigset, SIGINT);
    sigaddset(&stop_sigset, SIGRTSTOP);

    int sig;
    sigwait(&stop_sigset, &sig);

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

    sigaction(SIGTICK, &action, NULL);
    sigaction(SIGRESUME, &action, NULL);
    sigaction(SIGSUSPEND, &action, NULL);
    sigaction(SIGSYSCALL, &action, NULL);
    sigaction(SIGWAIT, &action, NULL);

    // Re-enable all signals.
    rt_interrupt_enable();

    // Restore the default handlers.
    action.sa_handler = SIG_DFL;
    sigaction(SIGTICK, &action, NULL);
    sigaction(SIGRESUME, &action, NULL);
    sigaction(SIGSUSPEND, &action, NULL);
    sigaction(SIGSYSCALL, &action, NULL);
    sigaction(SIGWAIT, &action, NULL);
}

void rt_stop(void)
{
    rt_interrupt_disable();
    pthread_kill(main_thread, SIGRTSTOP);
}
