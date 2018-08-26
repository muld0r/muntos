#include <rt/context.h>

#include <signal.h>
#include <stdlib.h>

#include <stdio.h>
#include <assert.h>

struct pthread_arg
{
  void (*fn)(void *);
  void *arg;
};

#define SIGSUSPEND SIGUSR1
#define SIGRESUME SIGUSR2

static pthread_mutex_t thread_lock;

static void wait_for_resume(void)
{
  assert(pthread_mutex_unlock(&thread_lock) == 0);
  sigset_t sigset;
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGRESUME);
  int sig;
  assert(sigwait(&sigset, &sig) == 0);
  printf("resumed %p\n", (void *)pthread_self());
  assert(pthread_mutex_lock(&thread_lock) == 0);
}

static void suspend_handler(int sig)
{
  (void)sig;
  printf("SIGSUSPEND to %p\n", (void *)pthread_self());
  wait_for_resume();
}

static void *pthread_fn(void *arg)
{
  assert(pthread_mutex_lock(&thread_lock) == 0);
  struct pthread_arg *parg = arg;
  void (*cfn)(void *) = parg->fn;
  void *carg = parg->arg;
  free(parg);
  wait_for_resume();
  cfn(carg);
  return NULL;
}

static void thread_init(void)
{
  // block SIGRESUME for all threads, it should only be consumed by sigwait
  sigset_t sigset;
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGRESUME);
  pthread_sigmask(SIG_BLOCK, &sigset, NULL);

  pthread_mutex_init(&thread_lock, NULL);
  // acquire the lock in the main context so it can be suspended
  assert(pthread_mutex_lock(&thread_lock) == 0);
}

void rt_context_init(rt_context_t *ctx, void *stack, size_t stack_size,
                      void (*fn)(void *), void *arg)
{
  static pthread_once_t thread_init_once = PTHREAD_ONCE_INIT;
  pthread_once(&thread_init_once, thread_init);

  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setstack(&attr, stack, stack_size);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  struct pthread_arg *parg = malloc(sizeof(*parg));
  parg->fn = fn;
  parg->arg = arg;
  struct sigaction suspend_action = {
      .sa_handler = suspend_handler,
  };
  sigemptyset(&suspend_action.sa_mask);
  sigaction(SIGSUSPEND, &suspend_action, NULL);
  pthread_create(ctx, &attr, pthread_fn, parg);
  assert(pthread_mutex_unlock(&thread_lock) == 0);
  assert(pthread_mutex_lock(&thread_lock) == 0);
  printf("created context %p\n", (void *)*ctx);
  pthread_attr_destroy(&attr);
}

void rt_context_swap(rt_context_t *old_ctx, const rt_context_t *new_ctx)
{
  printf("resuming %p, suspending %p\n", (void *)*new_ctx, (void *)pthread_self());
  pthread_kill(*new_ctx, SIGRESUME);
  *old_ctx = pthread_self();
  pthread_kill(*old_ctx, SIGSUSPEND);
}
