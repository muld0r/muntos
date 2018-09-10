#pragma once

#include <limits.h>

typedef struct rt_context
{
  void *thread;
  void *cond;
} rt_context_t;

void rt_port_start(void);

void rt_port_stop(void);

#define RT_STACK_MIN PTHREAD_STACK_MIN
