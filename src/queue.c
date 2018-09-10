#include <rt/queue.h>

#include <rt/critical.h>

#include <string.h>

void rt_queue_init(struct rt_queue *queue, const struct rt_queue_config *cfg)
{
  list_init(&queue->recv_list);
  list_init(&queue->send_list);
  queue->buf = cfg->buf;
  queue->len = 0;
  queue->write_offset = 0;
  queue->capacity = cfg->max_elems * cfg->elem_size;
  queue->elem_size = cfg->elem_size;
}

bool rt_queue_send(struct rt_queue *queue, const void *elem,
                    rt_tick_t timeout)
{
  rt_critical_begin();
  if (queue->len == queue->capacity)
  {
    (void)timeout;
    // TODO
  }
  memcpy(queue->buf + queue->write_offset, elem, queue->elem_size);
  queue->len += queue->elem_size;
  queue->write_offset += queue->elem_size;
  if (queue->write_offset == queue->capacity)
  {
    queue->write_offset = 0;
  }
  // TODO: notify tasks waiting for recv
  rt_critical_end();
  return true;
}

bool rt_queue_recv(struct rt_queue *queue, void *elem, rt_tick_t timeout)
{
  rt_critical_begin();
  if (queue->len == 0)
  {
    (void)timeout;
    // TODO
  }
  memcpy(elem, queue->buf + queue->read_offset, queue->elem_size);
  queue->len -= queue->elem_size;
  queue->read_offset += queue->elem_size;
  if (queue->read_offset == queue->capacity)
  {
    queue->read_offset = 0;
  }
  // TODO: notify tasks waiting for send
  rt_critical_end();
  return true;
}
