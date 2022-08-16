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

void rt_queue_send(struct rt_queue *queue, const void *elem)
{
    // TODO: don't use a critical section to protect the queue
    rt_critical_begin();
    if (queue->len == queue->capacity)
    {
        rt_list_push_back(&queue->send_list, &rt_task_self()->list);
        rt_yield();
        rt_critical_end();
        rt_critical_begin();
    }
    if (elem && queue->buf)
    {
        unsigned char *p = queue->buf;
        p += queue->write_offset;
        memcpy(p, elem, queue->elem_size);
    }
    queue->len += queue->elem_size;
    queue->write_offset += queue->elem_size;
    if (queue->write_offset == queue->capacity)
    {
        queue->write_offset = 0;
    }
    if (!rt_list_is_empty(&queue->recv_list))
    {
        struct rt_list *const node = rt_list_pop_front(&queue->recv_list);
        struct rt_task *const waiting_task =
            rt_list_item(node, struct rt_task, list);
        rt_task_ready(waiting_task);
    }
    rt_critical_end();
}

void rt_queue_recv(struct rt_queue *queue, void *elem)
{
    rt_critical_begin();
    if (queue->len == 0)
    {
        rt_list_push_back(&queue->recv_list, &rt_task_self()->list);
        rt_yield();
        rt_critical_end();
        rt_critical_begin();
    }
    if (elem && queue->buf)
    {
        const unsigned char *p = queue->buf;
        p += queue->read_offset;
        memcpy(elem, p, queue->elem_size);
    }
    queue->len -= queue->elem_size;
    queue->read_offset += queue->elem_size;
    if (queue->read_offset == queue->capacity)
    {
        queue->read_offset = 0;
    }
    if (!rt_list_is_empty(&queue->send_list))
    {
        struct rt_list *const node = rt_list_pop_front(&queue->send_list);
        struct rt_task *const waiting_task =
            rt_list_item(node, struct rt_task, list);
        rt_task_ready(waiting_task);
    }
    rt_critical_end();
}
