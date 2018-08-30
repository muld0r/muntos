#pragma once

#include <limits.h>
#include <pthread.h>

typedef struct rt_context
{
  pthread_t thread;
  pthread_cond_t *cond;
} rt_context_t;

void rt_port_start(void);

#define RT_CONTEXT_STACK_MIN PTHREAD_STACK_MIN
