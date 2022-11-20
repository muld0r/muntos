#include <rt/queue.h>

#include <rt/log.h>
#include <rt/task.h>

#include <string.h>

/* An empty slot, ready to receive new data. */
#define SLOT_EMPTY '\0'

/* An empty slot that has been claimed by a sender. */
#define SLOT_SEND 's'

/* A full slot that has been claimed by a receiver. */
#define SLOT_RECV 'r'

/* A full slot ready to be received. */
#define SLOT_FULL 'f'

/* An empty slot claimed by a sender and canceled by a receiver. The slot is
 * still exclusively owned by the sender, but the sender must clear it and
 * re-attempt inserting at the new enqueue index. */
#define SLOT_CANCELED 'x'

static size_t next(const struct rt_queue *queue, size_t i)
{
    i += 1;
    if (i == queue->num_elems)
    {
        return 0;
    }
    return i;
}

static void send(struct rt_queue *queue, const void *elem)
{
    size_t enq = rt_atomic_load_explicit(&queue->enq, memory_order_acquire);
    size_t start_enq = start_enq;
    for (;;)
    {
        size_t next_enq = next(queue, enq);
        char slot =
            rt_atomic_load_explicit(&queue->slots[enq], memory_order_relaxed);
        if ((slot == SLOT_EMPTY) &&
            rt_atomic_compare_exchange_strong_explicit(&queue->slots[enq],
                                                       &slot, SLOT_SEND,
                                                       memory_order_relaxed,
                                                       memory_order_relaxed))
        {
            unsigned char *const p = queue->data;
            memcpy(&p[queue->elem_size * enq], elem, queue->elem_size);
            slot = SLOT_SEND;
            if (!rt_atomic_compare_exchange_strong_explicit(
                    &queue->slots[enq], &slot, SLOT_FULL, memory_order_release,
                    memory_order_relaxed))
            {
                /* If our slot has been canceled by a reader, then restore it
                 * back to empty and start over. */
                rt_atomic_store_explicit(&queue->slots[enq], SLOT_EMPTY,
                                         memory_order_release);
                start_enq =
                    rt_atomic_load_explicit(&queue->enq, memory_order_acquire);
                enq = start_enq;
                continue;
            }
            /* Update enqueue index if no one has yet. */
            rt_atomic_compare_exchange_strong_explicit(&queue->enq, &start_enq,
                                                       next_enq,
                                                       memory_order_release,
                                                       memory_order_relaxed);
            break;
        }
        enq = next_enq;
    }
    rt_sem_post(&queue->recv_sem);
}

static void recv(struct rt_queue *queue, void *elem)
{
    size_t deq = rt_atomic_load_explicit(&queue->deq, memory_order_relaxed);
    size_t start_deq = deq;
    for (;;)
    {
        size_t next_deq = next(queue, deq);
        char slot =
            rt_atomic_load_explicit(&queue->slots[deq], memory_order_relaxed);
        if (slot == SLOT_SEND)
        {
            /* If we encounter an in-progress send, attempt to cancel it and
             * retry. The send may have completed on retry. If not, there will
             * be full slots to receive after this slot, because the level
             * allowing receivers to run is only incremented after each send is
             * complete. */
            rt_atomic_compare_exchange_strong_explicit(&queue->slots[deq],
                                                       &slot, SLOT_CANCELED,
                                                       memory_order_relaxed,
                                                       memory_order_relaxed);
            continue;
        }
        else if ((slot == SLOT_FULL) &&
                 rt_atomic_compare_exchange_strong_explicit(
                     &queue->slots[deq], &slot, SLOT_RECV, memory_order_acquire,
                     memory_order_relaxed))
        {
            const unsigned char *const p = queue->data;
            memcpy(elem, &p[queue->elem_size * deq], queue->elem_size);
            rt_atomic_store_explicit(&queue->slots[deq], SLOT_EMPTY,
                                     memory_order_relaxed);
            /* Update dequeue index if no one has yet. */
            rt_atomic_compare_exchange_strong_explicit(&queue->deq, &start_deq,
                                                       next_deq,
                                                       memory_order_relaxed,
                                                       memory_order_relaxed);
            break;
        }
        deq = next_deq;
    }
    rt_sem_post(&queue->send_sem);
}

void rt_queue_send(struct rt_queue *queue, const void *elem)
{
    rt_sem_wait(&queue->send_sem);
    send(queue, elem);
}

void rt_queue_recv(struct rt_queue *queue, void *elem)
{
    rt_sem_wait(&queue->recv_sem);
    recv(queue, elem);
}

bool rt_queue_trysend(struct rt_queue *queue, const void *elem)
{
    if (!rt_sem_trywait(&queue->send_sem))
    {
        return false;
    }
    send(queue, elem);
    return true;
}

bool rt_queue_tryrecv(struct rt_queue *queue, void *elem)
{
    if (!rt_sem_trywait(&queue->recv_sem))
    {
        return false;
    }
    recv(queue, elem);
    return true;
}

bool rt_queue_timedsend(struct rt_queue *queue, const void *elem,
                        unsigned long ticks)
{
    if (!rt_sem_timedwait(&queue->send_sem, ticks))
    {
        return false;
    }
    send(queue, elem);
    return true;
}

bool rt_queue_timedrecv(struct rt_queue *queue, void *elem, unsigned long ticks)
{
    if (!rt_sem_timedwait(&queue->recv_sem, ticks))
    {
        return false;
    }
    recv(queue, elem);
    return true;
}
