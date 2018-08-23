#pragma once

#include <limits.h>
#include <pthread.h>

typedef pthread_t context_t;

#define RT_CONTEXT_STACK_MIN PTHREAD_STACK_MIN
