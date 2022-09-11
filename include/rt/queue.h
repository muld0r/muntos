#ifndef RT_QUEUE_H
#define RT_QUEUE_H

#include <rt/cond.h>
#include <rt/mutex.h>

#include <stddef.h>

struct rt_queue;

void rt_queue_init(struct rt_queue *queue, void *buf, size_t num_elems,
                    size_t elem_len);

void rt_queue_recv(struct rt_queue *queue, void *elem);

void rt_queue_send(struct rt_queue *queue, const void *elem);

struct rt_queue
{
    struct rt_mutex mutex;
    struct rt_cond recv_ready;
    struct rt_cond send_ready;
    void *buf;
    size_t len;
    size_t recv_offset;
    size_t send_offset;
    size_t capacity;
    size_t elem_len;
};

#define RT_QUEUE_FROM_ARRAY(name, array)                                      \
    struct rt_queue name = {                                                  \
        .mutex = RT_MUTEX_INIT(name.mutex),                                        \
        .recv_ready = RT_COND_INIT(name.recv_ready),                               \
        .send_ready = RT_COND_INIT(name.send_ready),                               \
        .buf = array,                                                          \
        .len = 0,                                                              \
        .recv_offset = 0,                                                      \
        .send_offset = 0,                                                      \
        .capacity = sizeof(array),                                             \
        .elem_len = sizeof((array)[0]),                                        \
    }

#endif /* RT_QUEUE_H */
