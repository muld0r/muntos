#include <rt/context.h>

#include <semaphore.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>

struct pthread_arg
{
  void (*fn)(void *);
  void *arg;
  rt_context_t *ctx;
};

static pthread_mutex_t thread_lock;
static sem_t *thread_start_sem;

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
  cfn(carg);
  pthread_mutex_unlock(&thread_lock);
  return NULL;
}

static void thread_init(void)
{
  thread_start_sem = sem_open("thread_start_sem", O_CREAT, S_IRWXU, 0);

  pthread_mutex_init(&thread_lock, NULL);
  // acquire the lock in the main context so it can be suspended
  pthread_mutex_lock(&thread_lock);
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

  pthread_mutex_unlock(&thread_lock);
  pthread_create(&ctx->thread, &attr, pthread_fn, parg);
  sem_wait(thread_start_sem);
  pthread_mutex_lock(&thread_lock);

  pthread_attr_destroy(&attr);
}

void rt_context_swap(rt_context_t *old_ctx, const rt_context_t *new_ctx)
{
  if (old_ctx == new_ctx)
  {
    return;
  }
  pthread_cond_t cond;
  pthread_cond_init(&cond, NULL);
  old_ctx->thread = pthread_self();
  old_ctx->cond = &cond;
  pthread_cond_signal(new_ctx->cond);
  pthread_cond_wait(old_ctx->cond, &thread_lock);
}
