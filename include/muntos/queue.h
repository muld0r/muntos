#ifndef RT_QUEUE_H
#define RT_QUEUE_H

/*
 * A bounded, multi-producer, multi-consumer, lock-free queue that supports
 * blocking, timed, and non-blocking push, pop, and peek. The queue supports as
 * many threads/interrupts accessing it simultaneously as there are slots in
 * the queue. Additional queue operations will block, time out, or fail until a
 * previous thread has completed its operation. A push will block, time out, or
 * fail if there is no space left in the queue, and likewise for pop/peek if
 * the queue is empty.
 */

#include <muntos/atomic.h>
#include <muntos/sem.h>

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>

struct rt_queue;

void rt_queue_push(struct rt_queue *queue, const void *elem);

void rt_queue_pop(struct rt_queue *queue, void *elem);

void rt_queue_peek(struct rt_queue *queue, void *elem);

bool rt_queue_trypush(struct rt_queue *queue, const void *elem);

bool rt_queue_trypop(struct rt_queue *queue, void *elem);

bool rt_queue_trypeek(struct rt_queue *queue, void *elem);

bool rt_queue_timedpush(struct rt_queue *queue, const void *elem,
                        unsigned long ticks);

bool rt_queue_timedpop(struct rt_queue *queue, void *elem, unsigned long ticks);

bool rt_queue_timedpeek(struct rt_queue *queue, void *elem,
                        unsigned long ticks);

struct rt_queue
{
    struct rt_sem push_sem;
    struct rt_sem pop_sem;
    rt_atomic_size_t enq, deq;
    rt_atomic_uchar *slots;
    void *data;
    size_t num_elems, elem_size;
};

#define RT_QUEUE_STATE_BITS 4
#define RT_QUEUE_GEN_BITS (CHAR_BIT - RT_QUEUE_STATE_BITS)
#define RT_QUEUE_SIZE_BITS (sizeof(size_t) * CHAR_BIT)
#define RT_QUEUE_INDEX_BITS (RT_QUEUE_SIZE_BITS - RT_QUEUE_GEN_BITS)
#define RT_QUEUE_MAX_SIZE (((size_t)1 << RT_QUEUE_INDEX_BITS) - 1)

#define RT_QUEUE_STATIC(name, type, num)                                       \
    static_assert((num) <= RT_QUEUE_MAX_SIZE, "queue is too large");           \
    static type name##_elems[(num)];                                           \
    static rt_atomic_uchar name##_slots[(num)];                                \
    static struct rt_queue name = {                                            \
        .push_sem = RT_SEM_INIT(name.push_sem, (num)),                         \
        .pop_sem = RT_SEM_INIT(name.pop_sem, 0),                               \
        .enq = 0,                                                              \
        .deq = 0,                                                              \
        .slots = name##_slots,                                                 \
        .data = name##_elems,                                                  \
        .num_elems = (num),                                                    \
        .elem_size = sizeof(type),                                             \
    }

#endif /* RT_QUEUE_H */
