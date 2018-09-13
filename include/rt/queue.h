#pragma once

#include <rt/rt.h>

#include <stdbool.h>
#include <stddef.h>

struct rt_queue_config
{
  void *buf;
  size_t elem_size;
  size_t num_elems;
};

typedef struct rt_queue rt_queue_t;

void rt_queue_init(rt_queue_t *queue, const struct rt_queue_config *cfg);

bool rt_queue_send(rt_queue_t *queue, const void *elem, rt_tick_t timeout);

bool rt_queue_recv(rt_queue_t *queue, void *elem, rt_tick_t timeout);

struct rt_queue
{
  struct list recv_list;
  struct list send_list;
  char *buf;
  size_t len;
  size_t read_offset;
  size_t write_offset;
  size_t capacity;
  size_t elem_size;
};
