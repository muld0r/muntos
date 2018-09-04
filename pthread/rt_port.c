#include <rt/context.h>
#include <rt/port.h>

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
  rt_enable_interrupts();
  cfn(carg);
  pthread_mutex_unlock(&thread_lock);
  return NULL;
}

static void thread_init(void)
{
  thread_start_sem = sem_open("thread_start_sem", O_CREAT, S_IRWXU, 0);
  pthread_mutex_init(&thread_lock, NULL);
  rt_disable_interrupts();
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

void rt_context_swap(rt_context_t *old_ctx, const rt_context_t *new_ctx)
{
  if (old_ctx == new_ctx)
  {
    return;
  }

  rt_disable_interrupts();
  pthread_cond_t cond;
  pthread_cond_init(&cond, NULL);
  old_ctx->thread = pthread_self();
  old_ctx->cond = &cond;
  pthread_cond_signal(new_ctx->cond);
  pthread_cond_wait(old_ctx->cond, &thread_lock);
  rt_enable_interrupts();
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

  ualarm(1000, 1000);
  raise(SIGALRM); // tick immediately
  rt_enable_interrupts();
}
