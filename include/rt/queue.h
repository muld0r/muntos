#ifndef RT_QUEUE_H
#define RT_QUEUE_H

#include <rt/atomic.h>
#include <rt/sem.h>

#include <stdbool.h>
#include <stddef.h>

struct rt_queue
{
    struct rt_sem send_sem;
    struct rt_sem recv_sem;
    rt_atomic_size_t enq, deq;
    rt_atomic_char *slots;
    void *data;
    size_t num_elems, elem_size;
};

#define RT_QUEUE_STATIC(name, type, num)                                       \
    static type name##_elems[(num)];                                           \
    static rt_atomic_char name##_slots[(num)];                                 \
    static struct rt_queue name = {                                            \
        .send_sem = RT_SEM_INIT(name.send_sem, (num)),                         \
        .recv_sem = RT_SEM_INIT(name.recv_sem, 0),                             \
        .enq = 0,                                                              \
        .deq = 0,                                                              \
        .slots = name##_slots,                                                 \
        .data = name##_elems,                                                  \
        .num_elems = (num),                                                    \
        .elem_size = sizeof(type),                                             \
    }

void rt_queue_send(struct rt_queue *queue, const void *elem);

void rt_queue_recv(struct rt_queue *queue, void *elem);

bool rt_queue_trysend(struct rt_queue *queue, const void *elem);

bool rt_queue_tryrecv(struct rt_queue *queue, void *elem);

bool rt_queue_timedsend(struct rt_queue *queue, const void *elem,
                        unsigned long ticks);

bool rt_queue_timedrecv(struct rt_queue *queue, void *elem,
                        unsigned long ticks);

#endif /* RT_QUEUE_H */
