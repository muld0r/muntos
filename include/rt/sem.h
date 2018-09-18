#pragma once

#include <rt/rt.h>

#include <rt/queue.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct rt_queue rt_sem_t;

void rt_sem_init(rt_sem_t *sem, size_t count);

void rt_sem_post(rt_sem_t *sem);

bool rt_sem_wait(rt_sem_t *sem, rt_tick_t timeout);

#define RT_SEM_INIT(name, count)                                              \
  {                                                                            \
    .recv_list = LIST_HEAD_INIT(name.recv_list),                               \
    .send_list = LIST_HEAD_INIT(name.send_list), .buf = NULL, .len = count,    \
    .read_offset = 0, .write_offset = 0, .capacity = SIZE_MAX, .elem_size = 0, \
  }

#define RT_SEM_INIT_BINARY(name, count)                                       \
  {                                                                            \
    .recv_list = LIST_HEAD_INIT(name.recv_list),                               \
    .send_list = LIST_HEAD_INIT(name.send_list), .buf = NULL, .len = count,    \
    .read_offset = 0, .write_offset = 0, .capacity = 1, .elem_size = 0,        \
  }
