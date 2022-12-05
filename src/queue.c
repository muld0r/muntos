#include <rt/atomic.h>
#include <rt/queue.h>

#include <rt/log.h>
#include <rt/task.h>

#include <limits.h>
#include <stdatomic.h>
#include <string.h>

/* An empty slot, ready to receive new data. */
#define SLOT_EMPTY 0x00U

/* An empty slot that has been claimed by a sender. */
#define SLOT_SEND 0x05U

/* A full slot that has been claimed by a receiver/peeker. */
#define SLOT_RECV 0x0AU

/* A full slot ready to be received. */
#define SLOT_FULL 0x0FU

#define SLOT_STATE_MASK 0x0FU

#define STATE_BITS 4
#define GEN_BITS (8 - STATE_BITS)

#define SLOT_GEN_INCREMENT (1U << STATE_BITS)
#define SLOT_GEN_MASK (0xFFU & ~SLOT_STATE_MASK)

#define SIZE_BITS (sizeof(size_t) * CHAR_BIT)
#define INDEX_BITS (SIZE_BITS - GEN_BITS)
#define Q_GEN_INCREMENT ((size_t)1 << INDEX_BITS)
#define Q_INDEX_MASK (Q_GEN_INCREMENT - (size_t)1)
#define Q_GEN_MASK (~Q_INDEX_MASK)

static inline unsigned char state(unsigned char slot)
{
    return slot & SLOT_STATE_MASK;
}

static inline unsigned char sgen(unsigned char slot)
{
    return slot & SLOT_GEN_MASK;
}

static inline size_t qgen(size_t q)
{
    return q & Q_GEN_MASK;
}

static inline unsigned char qsgen(size_t q)
{
    return (unsigned char)(qgen(q) >> (INDEX_BITS - STATE_BITS));
}

static inline size_t index(size_t q)
{
    return q & Q_INDEX_MASK;
}

static const char *state_str(unsigned char state)
{
    switch (state)
    {
    case SLOT_EMPTY:
        return "empty";
    case SLOT_SEND:
        return "send";
    case SLOT_RECV:
        return "recv";
    case SLOT_FULL:
        return "full";
    default:
        return "unknown";
    }
}

static size_t next(size_t q, size_t num_elems)
{
    q += 1;
    if (index(q) == num_elems)
    {
        return qgen(q + Q_GEN_INCREMENT);
    }
    return q;
}

static void advance(atomic_size_t *q, size_t num_elems)
{
    size_t e = rt_atomic_load_explicit(q, memory_order_relaxed);
    while (!rt_atomic_compare_exchange_weak_explicit(q, &e, next(e, num_elems),
                                                     memory_order_relaxed,
                                                     memory_order_relaxed))
    {
    }
}

static void send(struct rt_queue *queue, const void *elem)
{
    for (;;)
    {
        size_t enq = rt_atomic_load_explicit(&queue->enq, memory_order_relaxed);

        /* Load the slot and attempt to claim it if it's empty. */
        atomic_uchar *slot = &queue->slots[index(enq)];
        unsigned char s = rt_atomic_load_explicit(slot, memory_order_relaxed);
        if (sgen(s) != qsgen(enq))
        {
            continue;
        }

        const unsigned char send_s = sgen(s) | SLOT_SEND;
        rt_logf("send: slot %zu %s\n", index(enq), state_str(state(s)));
        if ((state(s) == SLOT_EMPTY) &&
            rt_atomic_compare_exchange_strong_explicit(slot, &s, send_s,
                                                       memory_order_relaxed,
                                                       memory_order_relaxed))
        {
            rt_logf("send: slot %zu claimed...\n", index(enq));
            advance(&queue->enq, queue->num_elems);

            s = send_s;
            unsigned char *const p = queue->data;
            memcpy(&p[queue->elem_size * index(enq)], elem, queue->elem_size);

            /* Use release order even on failure so that the memcpy is complete
             * before we mark the slot as empty again. */
            if (rt_atomic_compare_exchange_strong_explicit(
                    slot, &s, sgen(s) | SLOT_FULL, memory_order_release,
                    memory_order_release))
            {
                break;
            }

            rt_logf("send: slot %zu skipped...\n", index(enq));
            /* If our slot has been skipped by a reader, then restore it
             * back to empty and keep looking. */
            rt_atomic_store_explicit(slot, sgen(s) | SLOT_EMPTY,
                                     memory_order_relaxed);
        }
    }
    rt_sem_post(&queue->recv_sem);
}

static void recv(struct rt_queue *queue, void *elem)
{
    for (;;)
    {
        size_t deq = rt_atomic_load_explicit(&queue->deq, memory_order_relaxed);

        atomic_uchar *slot = &queue->slots[index(deq)];
        unsigned char s = rt_atomic_load_explicit(slot, memory_order_relaxed);

        if (sgen(s) != qsgen(deq))
        {
            continue;
        }

        rt_logf("recv: slot %zu %s\n", index(deq), state_str(state(s)));
        if (state(s) == SLOT_SEND)
        {
            const unsigned char skipped_slot = s + SLOT_GEN_INCREMENT;
            /* If we encounter an in-progress send, attempt to skip it by
             * incrementing its generation. The send may have completed in the
             * mean time, in which case we can attempt to read from the slot.
             * If not, there will be full slots to receive after this slot,
             * because the level allowing receivers to run is only incremented
             * after each send is complete. */
            if (rt_atomic_compare_exchange_strong_explicit(
                    slot, &s, skipped_slot, memory_order_relaxed,
                    memory_order_relaxed))
            {
                /* If we successfully skipped the slot, advance the dequeue
                 * index. */
                advance(&queue->deq, queue->num_elems);
            }
        }
        else if ((state(s) == SLOT_FULL) &&
                 rt_atomic_compare_exchange_strong_explicit(
                     slot, &s, sgen(s) | SLOT_RECV, memory_order_acquire,
                     memory_order_relaxed))
        {
            rt_logf("recv: slot %zu claimed...\n", index(deq));
            advance(&queue->deq, queue->num_elems);
            const unsigned char *const p = queue->data;
            memcpy(elem, &p[queue->elem_size * index(deq)], queue->elem_size);
            const unsigned char empty_s =
                (sgen(s) + SLOT_GEN_INCREMENT) | SLOT_EMPTY;
            rt_atomic_store_explicit(slot, empty_s, memory_order_relaxed);
            break;
        }
    }
    rt_sem_post(&queue->send_sem);
}

static void peek(struct rt_queue *queue, void *elem)
{
    /* Similar to recv, but don't update the dequeue index and set the slot
     * back to SLOT_FULL after reading it so another receiver can read it. */
    for (;;)
    {
        size_t deq = rt_atomic_load_explicit(&queue->deq, memory_order_relaxed);

        atomic_uchar *slot = &queue->slots[index(deq)];
        unsigned char s = rt_atomic_load_explicit(slot, memory_order_relaxed);

        if (sgen(s) != qsgen(deq))
        {
            continue;
        }

        if ((state(s) == SLOT_FULL) &&
            rt_atomic_compare_exchange_strong_explicit(slot, &s,
                                                       sgen(s) | SLOT_RECV,
                                                       memory_order_acquire,
                                                       memory_order_relaxed))
        {
            const unsigned char *const p = queue->data;
            memcpy(elem, &p[queue->elem_size * index(deq)], queue->elem_size);
            rt_atomic_store_explicit(slot, sgen(s) | SLOT_FULL,
                                     memory_order_relaxed);
            break;
        }
    }
    /* After we've peeked, another receiver may run. */
    rt_sem_post(&queue->recv_sem);
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
