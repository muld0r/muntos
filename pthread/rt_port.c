#include <rt/context.h>
#include <rt/port.h>

#include <rt/rt.h>
#include <rt/critical.h>

#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

struct pthread_arg
{
  void (*fn)(void *);
  void *arg;
  void **ctx;
  void *parent_ctx;
};

static pthread_mutex_t thread_lock;

void rt_disable_interrupts(void)
{
  sigset_t sigset;
  sigfillset(&sigset);
  sigdelset(&sigset, SIGINT);
  pthread_sigmask(SIG_SETMASK, &sigset, NULL);
}

void rt_enable_interrupts(void)
{
  sigset_t sigset;
  sigemptyset(&sigset);
  pthread_sigmask(SIG_SETMASK, &sigset, NULL);
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
  pthread_cond_t cond;
  pthread_cond_init(&cond, NULL);
  parg->fn = fn;
  parg->arg = arg;
  parg->ctx = ctx;
  parg->parent_ctx = &cond;

  // launch each thread with signals blocked so only the active
  // thread will be delivered the SIGALRM
  rt_critical_begin();
  pthread_t thread;
  pthread_create(&thread, &attr, pthread_fn, parg);
  pthread_cond_wait(&cond, &thread_lock);
  rt_critical_end();

  free(parg);
  pthread_attr_destroy(&attr);
}

void rt_context_swap(void **old_ctx, void *new_ctx)
{
  pthread_cond_t cond;
  pthread_cond_init(&cond, NULL);
  *old_ctx = &cond;
  pthread_cond_signal(new_ctx);
  pthread_cond_wait(&cond, &thread_lock);
}

static void tick_handler(int sig)
{
  (void)sig;
  rt_tick();
}

void rt_port_start(void)
{
  pthread_once(&thread_init_once, thread_init);

  struct sigaction tick_action = {.sa_handler = tick_handler};
  sigemptyset(&tick_action.sa_mask);
  sigaction(SIGALRM, &tick_action, NULL);

  ualarm(1000, 1000);
}

void rt_port_stop(void)
{
  // prevent new SIGALRMs
  ualarm(0, 0);

  // change handler to SIG_IGN to drop any pending SIGALRM
  struct sigaction tick_action = {.sa_handler = SIG_IGN};
  sigemptyset(&tick_action.sa_mask);
  sigaction(SIGALRM, &tick_action, NULL);

  // restore the default handler
  tick_action.sa_handler = SIG_DFL;
  sigaction(SIGALRM, &tick_action, NULL);

  pthread_mutex_unlock(&thread_lock);
}
