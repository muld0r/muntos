#include "context_port.h"

#include <rt/context.h>

#include <stdlib.h>
#include <signal.h>

struct pthread_arg
{
  void (*fn)(void *);
  void *arg;
};

#define SIGSUSPEND SIGUSR1
#define SIGRESUME SIGUSR2

static void wait_for_resume(void)
{
  sigset_t sigset;
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGRESUME);
  int sig;
  sigwait(&sigset, &sig);
}

static void suspend_handler(int sig)
{
  (void)sig;
  wait_for_resume();
}

static void *pthread_fn(void *arg)
{
  struct pthread_arg *parg = arg;
  void (*cfn)(void *) = parg->fn;
  void *carg = parg->arg;
  free(parg);
  wait_for_resume();
  cfn(carg);
  return NULL;
}

void context_init(context_t *ctx, void *stack, size_t stack_size,
                  void (*fn)(void *), void *arg)
{
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setstack(&attr, stack, stack_size);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  struct pthread_arg *parg = malloc(sizeof(*parg));
  parg->fn = fn;
  parg->arg = arg;
  sigset_t sigset;
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGRESUME);
  // block the resume signal for all threads; it should only be consumed by sigwait
  pthread_sigmask(SIG_BLOCK, &sigset, NULL);
  struct sigaction suspend_action = {
    .sa_handler = suspend_handler,
  };
  sigemptyset(&suspend_action.sa_mask);
  sigaction(SIGSUSPEND, &suspend_action, NULL);
  pthread_create(ctx, &attr, pthread_fn, parg);
  pthread_attr_destroy(&attr);
}

void context_restore(const context_t *ctx)
{
  pthread_kill(*ctx, SIGRESUME);
  sched_yield();
}

void context_save(context_t *ctx)
{
  pthread_kill(*ctx, SIGSUSPEND);
}
