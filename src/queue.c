#include <rt/queue.h>

#include <string.h>

void rt_queue_init(struct rt_queue *queue, void *buf, size_t num_elems,
                    size_t elem_len)
{
    rt_mutex_init(&queue->mutex);
    rt_cond_init(&queue->recv_ready);
    rt_cond_init(&queue->send_ready);
    queue->buf = buf;
    queue->len = 0;
    queue->recv_offset = 0;
    queue->send_offset = 0;
    queue->capacity = num_elems * elem_len;
    queue->elem_len = elem_len;
}

void rt_queue_recv(struct rt_queue *queue, void *elem)
{
    rt_mutex_lock(&queue->mutex);
    while (queue->len == 0)
    {
        rt_cond_wait(&queue->recv_ready, &queue->mutex);
    }
    const unsigned char *p = queue->buf;
    p += queue->recv_offset;
    memcpy(elem, p, queue->elem_len);
    queue->len -= queue->elem_len;
    queue->recv_offset += queue->elem_len;
    if (queue->recv_offset == queue->capacity)
    {
        queue->recv_offset = 0;
    }

    /* TODO: more fine-grained */
    rt_cond_broadcast(&queue->send_ready);
    rt_mutex_unlock(&queue->mutex);
}

void rt_queue_send(struct rt_queue *queue, const void *elem)
{
    rt_mutex_lock(&queue->mutex);
    while (queue->len == queue->capacity)
    {
        rt_cond_wait(&queue->send_ready, &queue->mutex);
    }
    unsigned char *p = queue->buf;
    p += queue->send_offset;
    memcpy(p, elem, queue->elem_len);
    queue->len += queue->elem_len;
    queue->send_offset += queue->elem_len;
    if (queue->send_offset == queue->capacity)
    {
        queue->send_offset = 0;
    }

    /* TODO: more fine-grained */
    rt_cond_broadcast(&queue->recv_ready);
    rt_mutex_unlock(&queue->mutex);
}
