#pragma once

#include <rt/rt.h>

#include <rt/queue.h>

#include <stdbool.h>
#include <stddef.h>

typedef struct rt_queue rt_sem_t;

void rt_sem_init(rt_sem_t *sem, size_t count);

void rt_sem_post(rt_sem_t *sem);

bool rt_sem_wait(rt_sem_t *sem, rt_tick_t timeout);
