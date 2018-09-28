#include <rt/queue.h>

#include <rt/critical.h>
#include <rt/delay.h>

#include <string.h>

void rt_queue_init(struct rt_queue *queue, const struct rt_queue_config *cfg)
{
  list_head_init(&queue->recv_list);
  list_head_init(&queue->send_list);
  queue->buf = cfg->buf;
  queue->len = 0;
  queue->read_offset = 0;
  queue->write_offset = 0;
  queue->capacity = cfg->elem_size * cfg->num_elems;
  queue->elem_size = cfg->elem_size;
}

bool rt_queue_send(struct rt_queue *queue, const void *elem,
                    rt_tick_t timeout)
{
  bool success = true;
  rt_critical_begin();
  if (queue->len == queue->capacity)
  {
    list_add_tail(&queue->send_list, &rt_self()->event_list);
    rt_delay(timeout);
    if (queue->len == queue->capacity)
    {
      success = false;
      goto end;
    }
  }
  if (elem && queue->buf)
  {
    memcpy(queue->buf + queue->write_offset, elem, queue->elem_size);
  }
  queue->len += queue->elem_size;
  queue->write_offset += queue->elem_size;
  if (queue->write_offset == queue->capacity)
  {
    queue->write_offset = 0;
  }
  if (!list_empty(&queue->recv_list))
  {
    struct rt_task *waiting_task =
        list_item(list_front(&queue->recv_list), struct rt_task, event_list);
    rt_resume(waiting_task);
  }

end:
  rt_critical_end();
  return success;
}

bool rt_queue_recv(struct rt_queue *queue, void *elem, rt_tick_t timeout)
{
  bool success = true;
  rt_critical_begin();
  if (queue->len == 0)
  {
    list_add_tail(&queue->recv_list, &rt_self()->event_list);
    rt_delay(timeout);
    if (queue->len == 0)
    {
      success = false;
      goto end;
    }
  }
  if (elem && queue->buf)
  {
    memcpy(elem, queue->buf + queue->read_offset, queue->elem_size);
  }
  queue->len -= queue->elem_size;
  queue->read_offset += queue->elem_size;
  if (queue->read_offset == queue->capacity)
  {
    queue->read_offset = 0;
  }
  if (!list_empty(&queue->send_list))
  {
    struct rt_task *waiting_task =
        list_item(list_front(&queue->send_list), struct rt_task, event_list);
    rt_resume(waiting_task);
  }

end:
  rt_critical_end();
  return success;
}
