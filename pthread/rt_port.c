#include <rt/context.h>
#include <rt/critical.h>
#include <rt/port.h>
#include <rt/tick.h>

#include <rt/rt.h>

#include <semaphore.h>
#include <signal.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

struct pthread_arg
{
  void (*fn)(void *);
  void *arg;
  rt_context_t *ctx;
};

static pthread_mutex_t thread_lock;
static sem_t *thread_start_sem;

static void block_alarm(void)
{
  sigset_t sigset;
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGALRM);
  pthread_sigmask(SIG_BLOCK, &sigset, NULL);
}

static void unblock_alarm(void)
{
  sigset_t sigset;
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGALRM);
  pthread_sigmask(SIG_UNBLOCK, &sigset, NULL);
}

static void *pthread_fn(void *arg)
{
  pthread_cond_t cond;
  pthread_cond_init(&cond, NULL);
  pthread_mutex_lock(&thread_lock);
  struct pthread_arg *parg = arg;
  void (*cfn)(void *) = parg->fn;
  void *carg = parg->arg;
  parg->ctx->cond = &cond;
  free(parg);
  sem_post(thread_start_sem);
  pthread_cond_wait(&cond, &thread_lock);
  unblock_alarm();
  cfn(carg);
  pthread_mutex_unlock(&thread_lock);
  return NULL;
}

static void thread_init(void)
{
  thread_start_sem = sem_open("thread_start_sem", O_CREAT, S_IRWXU, 0);
  pthread_mutex_init(&thread_lock, NULL);
  block_alarm();
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
  parg->ctx = ctx;

  pthread_create(&ctx->thread, &attr, pthread_fn, parg);
  sem_wait(thread_start_sem);

  pthread_attr_destroy(&attr);
}

static uint32_t critical_nesting = 0;

void rt_critical_begin(void)
{
  block_alarm();
  ++critical_nesting;
}

void rt_critical_end(void)
{
  --critical_nesting;
  if (critical_nesting == 0)
  {
    unblock_alarm();
  }
}

void rt_context_swap(rt_context_t *old_ctx, const rt_context_t *new_ctx)
{
  if (old_ctx == new_ctx)
  {
    return;
  }

  block_alarm();
  pthread_cond_t cond;
  pthread_cond_init(&cond, NULL);
  old_ctx->thread = pthread_self();
  old_ctx->cond = &cond;
  pthread_cond_signal(new_ctx->cond);
  pthread_cond_wait(old_ctx->cond, &thread_lock);
  unblock_alarm();
}

static void tick_handler(int sig)
{
  (void)sig;
  rt_tick();
}

void rt_port_start(void)
{
  pthread_mutex_lock(&thread_lock);

  struct sigaction tick_action = {
      .sa_handler = tick_handler,
  };
  sigemptyset(&tick_action.sa_mask);
  sigaction(SIGALRM, &tick_action, NULL);

  ualarm(1, 1000);
  unblock_alarm();
}
