#include <rt/context.h>
#include <rt/port.h>

#include <rt/critical.h>
#include <rt/rt.h>

#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

struct pthread_arg
{
  void (*fn)(void *);
  void *arg;
  void **ctx;
  void *parent_ctx;
};

struct pthread_ctx
{
  pthread_cond_t cond;
  bool runnable;
};

static pthread_mutex_t thread_lock;

static void sig_interrupt_set(sigset_t *sigset)
{
  sigfillset(sigset);
  sigdelset(sigset, SIGINT);
}

void rt_disable_interrupts(void)
{
  sigset_t sigset;
  sig_interrupt_set(&sigset);
  pthread_sigmask(SIG_BLOCK, &sigset, NULL);
}

void rt_enable_interrupts(void)
{
  sigset_t sigset;
  sig_interrupt_set(&sigset);
  pthread_sigmask(SIG_UNBLOCK, &sigset, NULL);
}

static void *pthread_fn(void *arg)
{
  struct pthread_arg *parg = arg;
  void (*cfn)(void *) = parg->fn;
  void *carg = parg->arg;
  void **ctx = parg->ctx;
  void *parent_ctx = parg->parent_ctx;
  pthread_mutex_lock(&thread_lock);
  rt_context_swap(ctx, parent_ctx);
  cfn(carg);
  pthread_mutex_unlock(&thread_lock);
  return NULL;
}

static void thread_init(void)
{
  pthread_mutex_init(&thread_lock, NULL);
  pthread_mutex_lock(&thread_lock);
}

static pthread_once_t thread_init_once = PTHREAD_ONCE_INIT;

void rt_context_init(void **ctx, void *stack, size_t stack_size,
                      void (*fn)(void *), void *arg)
{
  pthread_once(&thread_init_once, thread_init);

  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setstack(&attr, stack, stack_size);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  struct pthread_arg *parg = malloc(sizeof(*parg));
  struct pthread_ctx pctx;
  pthread_cond_init(&pctx.cond, NULL);
  pctx.runnable = false;
  parg->fn = fn;
  parg->arg = arg;
  parg->ctx = ctx;
  parg->parent_ctx = &pctx;

  // launch each thread with signals blocked so only the active
  // thread will be delivered the SIGALRM
  rt_critical_begin();
  pthread_t thread;
  pthread_create(&thread, &attr, pthread_fn, parg);
  while (!pctx.runnable)
  {
    pthread_cond_wait(&pctx.cond, &thread_lock);
  }
  rt_critical_end();

  free(parg);
  pthread_attr_destroy(&attr);
}

void rt_context_swap(void **old_ctx, void *new_ctx)
{
  struct pthread_ctx octx, *nctx = new_ctx;
  pthread_cond_init(&octx.cond, NULL);
  octx.runnable = false;
  *old_ctx = &octx;
  nctx->runnable = true;
  pthread_cond_signal(&nctx->cond);
  rt_disable_interrupts();
  while (!octx.runnable)
  {
    pthread_cond_wait(&octx.cond, &thread_lock);
  }
  rt_enable_interrupts();
}

static void tick_handler(int sig)
{
  (void)sig;
  rt_tick();
}

void rt_port_start(void)
{
  pthread_once(&thread_init_once, thread_init);

  // list of signals in increasing priority order
  // each signal masks all signals before it in the list when active
  struct {
    void (*sigfn)(int);
    int sig;
  } signals[] = {
    {.sigfn = tick_handler, .sig = SIGALRM},
    {.sigfn = NULL, .sig = 0},
  };

  struct sigaction action = {.sa_handler = NULL};
  sigemptyset(&action.sa_mask);

  for (size_t i = 0; signals[i].sigfn != NULL; ++i)
  {
    action.sa_handler = signals[i].sigfn;
    sigaction(signals[i].sig, &action, NULL);
    sigaddset(&action.sa_mask, signals[i].sig);
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
}

void rt_port_stop(void)
{
  // prevent new SIGALRMs
  static const struct timeval zero = {
    .tv_sec = 0,
    .tv_usec = 0,
  };
  struct itimerval timer = {
    .it_interval = zero,
    .it_value = zero,
  };
  setitimer(ITIMER_REAL, &timer, NULL);

  // change handler to SIG_IGN to drop any pending SIGALRM
  struct sigaction tick_action = {.sa_handler = SIG_IGN};
  sigemptyset(&tick_action.sa_mask);
  sigaction(SIGALRM, &tick_action, NULL);

  // restore the default handler
  tick_action.sa_handler = SIG_DFL;
  sigaction(SIGALRM, &tick_action, NULL);

  pthread_mutex_unlock(&thread_lock);
}
