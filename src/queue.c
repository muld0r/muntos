#include <rt/queue.h>

#include <rt/critical.h>
#include <rt/sleep.h>

#include <string.h>

void rt_queue_init(struct rt_queue *queue, const struct rt_queue_config *cfg)
{
    rt_list_init(&queue->recv_list);
    rt_list_init(&queue->send_list);
    queue->buf = cfg->buf;
    queue->len = 0;
    queue->read_offset = 0;
    queue->write_offset = 0;
    queue->capacity = cfg->elem_size * cfg->num_elems;
    queue->elem_size = cfg->elem_size;
}

bool rt_queue_send(struct rt_queue *queue, const void *elem)
{
    (void)queue;
    (void)elem;
    return false;
#if 0
    bool success = true;
    rt_critical_begin();
    if (queue->len == queue->capacity)
    {
        if (timeout == 0)
        {
            success = false;
            goto end;
        }
        list_push_back(&queue->send_list, &rt_task_self()->event_list);
        rt_sleep(timeout);
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
    if (list_front(&queue->recv_list))
    {
        struct rt_task *waiting_task = list_item(list_front(&queue->recv_list),
                                                  struct rt_task, event_list);
        rt_task_resume(waiting_task);
    }

end:
    rt_critical_end();
    return success;
#endif
}

bool rt_queue_recv(struct rt_queue *queue, void *elem)
{
    (void)queue;
    (void)elem;
    return false;
#if 0
    bool success = true;
    rt_critical_begin();
    if (queue->len == 0)
    {
        if (timeout == 0)
        {
            success = false;
            goto end;
        }
        list_push_back(&queue->recv_list, &rt_task_self()->event_list);
        rt_sleep(timeout);
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
    if (list_front(&queue->send_list))
    {
        struct rt_task *waiting_task = list_item(list_front(&queue->send_list),
                                                  struct rt_task, event_list);
        rt_task_resume(waiting_task);
    }

end:
    rt_critical_end();
    return success;
#endif
}
