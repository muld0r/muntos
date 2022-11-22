#include <rt/queue.h>

#include <rt/log.h>
#include <rt/task.h>

#include <string.h>

/* An empty slot, ready to receive new data. */
#define SLOT_EMPTY '\0'

/* An empty slot that has been claimed by a sender. */
#define SLOT_SEND 's'

/* A full slot ready to be received. */
#define SLOT_FULL 'f'

/* A full slot that has been claimed by a receiver/peeker. */
#define SLOT_RECV 'p'

/* An empty slot claimed by a sender and canceled by a receiver/peeker. The
 * slot is still exclusively owned by the sender, but the sender must clear it
 * and re-attempt inserting at the new enqueue index. */
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
    for (;;)
    {
        size_t enq = rt_atomic_load_explicit(&queue->enq, memory_order_relaxed);
        rt_atomic_compare_exchange_strong_explicit(&queue->enq, &enq,
                                                   next(queue, enq),
                                                   memory_order_relaxed,
                                                   memory_order_relaxed);
        char slot =
            rt_atomic_load_explicit(&queue->slots[enq], memory_order_relaxed);
        rt_logf("send: slot %zu = %c\n", enq, slot);
        if ((slot == SLOT_EMPTY) &&
            rt_atomic_compare_exchange_strong_explicit(&queue->slots[enq],
                                                       &slot, SLOT_SEND,
                                                       memory_order_relaxed,
                                                       memory_order_relaxed))
        {
            rt_logf("send: slot %zu claimed...\n", enq);
            unsigned char *const p = queue->data;
            memcpy(&p[queue->elem_size * enq], elem, queue->elem_size);
            slot = SLOT_SEND;
            if (rt_atomic_compare_exchange_strong_explicit(
                    &queue->slots[enq], &slot, SLOT_FULL, memory_order_release,
                    memory_order_relaxed))
            {
                rt_sem_post(&queue->recv_sem);
                return;
            }
            rt_logf("send: slot %zu canceled...\n", enq);
            /* If our slot has been canceled by a reader, then restore it
             * back to empty and keep looking. */
            rt_atomic_store_explicit(&queue->slots[enq], SLOT_EMPTY,
                                     memory_order_relaxed);
        }
    }
}

static void recv(struct rt_queue *queue, void *elem)
{
    for (;;)
    {
        size_t deq = rt_atomic_load_explicit(&queue->deq, memory_order_relaxed);
        rt_atomic_compare_exchange_strong_explicit(&queue->deq, &deq,
                                                   next(queue, deq),
                                                   memory_order_relaxed,
                                                   memory_order_relaxed);
        char slot =
            rt_atomic_load_explicit(&queue->slots[deq], memory_order_relaxed);
        rt_logf("recv: slot %zu = %c\n", deq, slot);
        if (slot == SLOT_SEND)
        {
            /* If we encounter an in-progress send, attempt to cancel it. The
             * send may have completed in the mean time, in which case we can
             * attempt to read from the slot. If not, there will be full slots
             * to receive after this slot, because the level allowing receivers
             * to run is only incremented after each send is complete. */
            rt_atomic_compare_exchange_strong_explicit(&queue->slots[deq],
                                                       &slot, SLOT_CANCELED,
                                                       memory_order_relaxed,
                                                       memory_order_relaxed);
        }
        if ((slot == SLOT_FULL) &&
            rt_atomic_compare_exchange_strong_explicit(&queue->slots[deq],
                                                       &slot, SLOT_RECV,
                                                       memory_order_acquire,
                                                       memory_order_relaxed))
        {
            rt_logf("recv: slot %zu claimed...\n", deq);
            /* Update dequeue index if no one has yet. */
            const unsigned char *const p = queue->data;
            memcpy(elem, &p[queue->elem_size * deq], queue->elem_size);
            rt_atomic_store_explicit(&queue->slots[deq], SLOT_EMPTY,
                                     memory_order_relaxed);
            rt_sem_post(&queue->send_sem);
            return;
        }
    }
}

static void peek(struct rt_queue *queue, void *elem)
{
    /* Similar to recv, but don't update the dequeue index and set the slot
     * back to SLOT_FULL after reading it so another receiver can read it. */
    size_t deq = rt_atomic_load_explicit(&queue->deq, memory_order_relaxed);
    for (;;)
    {
        char slot =
            rt_atomic_load_explicit(&queue->slots[deq], memory_order_relaxed);
        if (slot == SLOT_SEND)
        {
            rt_atomic_compare_exchange_strong_explicit(&queue->slots[deq],
                                                       &slot, SLOT_CANCELED,
                                                       memory_order_relaxed,
                                                       memory_order_relaxed);
        }
        if ((slot == SLOT_FULL) &&
            rt_atomic_compare_exchange_strong_explicit(&queue->slots[deq],
                                                       &slot, SLOT_RECV,
                                                       memory_order_acquire,
                                                       memory_order_relaxed))
        {
            const unsigned char *const p = queue->data;
            memcpy(elem, &p[queue->elem_size * deq], queue->elem_size);
            rt_atomic_store_explicit(&queue->slots[deq], SLOT_FULL,
                                     memory_order_relaxed);
            /* After we've peeked, another receiver may run. */
            rt_sem_post(&queue->recv_sem);
            return;
        }
        deq = next(queue, deq);
    }
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

void rt_queue_peek(struct rt_queue *queue, void *elem)
{
    rt_sem_wait(&queue->recv_sem);
    peek(queue, elem);
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

bool rt_queue_trypeek(struct rt_queue *queue, void *elem)
{
    if (!rt_sem_trywait(&queue->recv_sem))
    {
        return false;
    }
    peek(queue, elem);
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

bool rt_queue_timedpeek(struct rt_queue *queue, void *elem, unsigned long ticks)
{
    if (!rt_sem_timedwait(&queue->recv_sem, ticks))
    {
        return false;
    }
    peek(queue, elem);
    return true;
}
