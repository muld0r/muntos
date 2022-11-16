#ifndef RT_QUEUE_H
#define RT_QUEUE_H

#include <rt/atomic.h>
#include <rt/list.h>
#include <rt/syscall.h>

#include <stdbool.h>
#include <stddef.h>

struct rt_queue
{
    rt_atomic_long level;
    rt_atomic_size_t enq, deq;
    rt_atomic_char *slots;
    void *data;
    size_t num_elems, elem_size;
    struct rt_list send_list, recv_list;
    struct rt_syscall_record wake_record;
    size_t num_senders, num_recvers;
    rt_atomic_flag wake_pending;
};

#define RT_QUEUE_STATIC(name, type, num)                                       \
    static type name##_elems[(num)];                                           \
    static rt_atomic_char name##_slots[(num)];                                 \
    static struct rt_queue name = {                                            \
        .level = 0,                                                            \
        .enq = 0,                                                              \
        .deq = 0,                                                              \
        .slots = name##_slots,                                                 \
        .data = name##_elems,                                                  \
        .num_elems = (num),                                                    \
        .elem_size = sizeof(type),                                             \
        .send_list = RT_LIST_INIT(name.send_list),                             \
        .recv_list = RT_LIST_INIT(name.recv_list),                             \
        .wake_record =                                                         \
            {                                                                  \
                .next = NULL,                                                  \
                .task = NULL,                                                  \
                .syscall = RT_SYSCALL_QUEUE_WAKE,                              \
                .args.queue = &name,                                           \
            },                                                                 \
        .num_senders = 0,                                                      \
        .num_recvers = 0,                                                      \
        .wake_pending = RT_ATOMIC_FLAG_INIT,                                   \
    }

void rt_queue_send(struct rt_queue *queue, const void *elem);

void rt_queue_recv(struct rt_queue *queue, void *elem);

bool rt_queue_trysend(struct rt_queue *queue, const void *elem);

bool rt_queue_tryrecv(struct rt_queue *queue, void *elem);

#endif /* RT_QUEUE_H */
