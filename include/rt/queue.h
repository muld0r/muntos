#pragma once

#include <rt/rt.h>

#include <stdbool.h>
#include <stddef.h>

struct rt_queue_config
{
  void *buf;
  size_t max_elems;
  size_t elem_size;
};

struct rt_queue
{
  struct list recv_list;
  struct list send_list;
  unsigned char *buf;
  size_t len;
  size_t read_offset;
  size_t write_offset;
  size_t capacity;
  size_t elem_size;
};

void rt_queue_init(struct rt_queue *queue,
                    const struct rt_queue_config *cfg);

bool rt_queue_send(struct rt_queue *queue, const void *elem,
                    rt_tick_t timeout);

bool rt_queue_recv(struct rt_queue *queue, void *elem, rt_tick_t timeout);
