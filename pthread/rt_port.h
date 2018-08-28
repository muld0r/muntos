#pragma once

#include <limits.h>
#include <pthread.h>

typedef struct {
  pthread_t thread;
  pthread_cond_t *cond;
} rt_context_t;

#define RT_CONTEXT_STACK_MIN PTHREAD_STACK_MIN
