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

/* An empty slot that has been skipped by a receiver. */
#define SLOT_SKIPPED 0x0CU

/* A full slot ready to be received. */
#define SLOT_FULL 0x0FU

#define SLOT_STATE_MASK 0x0FU

#define SLOT_GEN_INCREMENT (1U << RT_QUEUE_STATE_BITS)
#define SLOT_GEN_MASK (0xFFU & ~SLOT_STATE_MASK)

#define Q_GEN_INCREMENT ((size_t)1 << RT_QUEUE_INDEX_BITS)
#define Q_INDEX_MASK (Q_GEN_INCREMENT - (size_t)1)
#define Q_GEN_MASK (~Q_INDEX_MASK)
#define Q_SGEN_SHIFT (RT_QUEUE_INDEX_BITS - RT_QUEUE_STATE_BITS)

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
    return (unsigned char)(qgen(q) >> Q_SGEN_SHIFT);
}

static inline size_t qindex(size_t q)
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
    if (qindex(q) == num_elems)
    {
        return qgen(q + Q_GEN_INCREMENT);
    }
    return q;
}

static void send(struct rt_queue *queue, const void *elem)
{
    for (;;)
    {
        size_t enq = rt_atomic_load_explicit(&queue->enq, memory_order_relaxed);
        size_t last_enq = enq;
        atomic_uchar *slot;
        unsigned char s;
        for (;;)
        {
            slot = &queue->slots[qindex(enq)];
            s = rt_atomic_load_explicit(slot, memory_order_relaxed);
            rt_logf("send: slot %zu %s\n", qindex(enq), state_str(state(s)));
            if ((state(s) == SLOT_EMPTY) && (sgen(s) == qsgen(enq)))
            {
                break;
            }
            const size_t new_enq =
                rt_atomic_load_explicit(&queue->enq, memory_order_relaxed);
            if (new_enq != last_enq)
            {
                enq = new_enq;
                last_enq = new_enq;
            }
            else
            {
                enq = next(enq, queue->num_elems);
            }
        }

        const unsigned char send_s = sgen(s) | SLOT_SEND;
        if (rt_atomic_compare_exchange_strong_explicit(slot, &s, send_s,
                                                       memory_order_relaxed,
                                                       memory_order_relaxed))
        {
            rt_logf("send: slot %zu claimed...\n", qindex(enq));
            rt_atomic_store_explicit(&queue->enq, next(enq, queue->num_elems),
                                     memory_order_relaxed);

            unsigned char *const p = queue->data;
            memcpy(&p[queue->elem_size * qindex(enq)], elem, queue->elem_size);

            s = send_s;
            if (rt_atomic_compare_exchange_strong_explicit(
                    slot, &s, sgen(s) | SLOT_FULL, memory_order_release,
                    memory_order_relaxed))
            {
                break;
            }

            rt_logf("send: slot %zu skipped...\n", qindex(enq));
            /* If our slot has been skipped by a reader, then restore it
             * back to empty and keep looking. */
            while (!rt_atomic_compare_exchange_weak_explicit(
                slot, &s, sgen(s) | SLOT_EMPTY, memory_order_release,
                memory_order_relaxed))
            {
            }
        }
    }
    rt_sem_post(&queue->recv_sem);
}

static void recv(struct rt_queue *queue, void *elem)
{
    for (;;)
    {
        size_t deq = rt_atomic_load_explicit(&queue->deq, memory_order_relaxed);
        size_t last_deq = deq;
        atomic_uchar *slot;
        unsigned char s;
        for (;;)
        {
            slot = &queue->slots[qindex(deq)];
            s = rt_atomic_load_explicit(slot, memory_order_relaxed);
            rt_logf("recv: slot %zu %s\n", qindex(deq), state_str(state(s)));
            if (sgen(s) == qsgen(deq))
            {
                if (state(s) == SLOT_SEND)
                {
                    const unsigned char skipped_slot =
                        (sgen(s) + SLOT_GEN_INCREMENT) | SLOT_SKIPPED;
                    /* If we encounter an in-progress send, attempt to skip it.
                     * The send may have completed in the mean time, in which
                     * case we can attempt to read from the slot. If not, there
                     * will be full slots to receive after this slot, because
                     * the level allowing receivers to run is only incremented
                     * after each send is complete. */
                    if (rt_atomic_compare_exchange_strong_explicit(
                            slot, &s, skipped_slot, memory_order_relaxed,
                            memory_order_relaxed))
                    {
                        rt_logf("recv: slot %zu skipped...\n", qindex(deq));
                    }
                }
                if ((state(s) == SLOT_FULL) || (state(s) == SLOT_RECV))
                {
                    break;
                }
            }
            const size_t new_deq =
                rt_atomic_load_explicit(&queue->deq, memory_order_relaxed);
            if (new_deq != last_deq)
            {
                deq = new_deq;
                last_deq = new_deq;
            }
            else
            {
                deq = next(deq, queue->num_elems);
            }
        }

        unsigned char recv_s = sgen(s) | SLOT_RECV;
        if (rt_atomic_compare_exchange_strong_explicit(slot, &s, recv_s,
                                                       memory_order_acquire,
                                                       memory_order_relaxed))
        {
            rt_logf("recv: slot %zu claimed...\n", qindex(deq));

            const unsigned char *const p = queue->data;
            memcpy(elem, &p[queue->elem_size * qindex(deq)], queue->elem_size);

            const unsigned char empty_s =
                (sgen(s) + SLOT_GEN_INCREMENT) | SLOT_EMPTY;
            if (rt_atomic_compare_exchange_strong_explicit(
                    slot, &recv_s, empty_s, memory_order_relaxed,
                    memory_order_relaxed))
            {
                rt_atomic_store_explicit(&queue->deq,
                                         next(deq, queue->num_elems),
                                         memory_order_relaxed);
                break;
            }
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
        size_t last_deq = deq;
        atomic_uchar *slot;
        unsigned char s;
        for (;;)
        {
            slot = &queue->slots[qindex(deq)];
            s = rt_atomic_load_explicit(slot, memory_order_relaxed);
            rt_logf("peek: slot %zu %s\n", qindex(deq), state_str(state(s)));
            if (sgen(s) == qsgen(deq))
            {
                if ((state(s) == SLOT_FULL) || (state(s) == SLOT_RECV))
                {
                    break;
                }
            }
            const size_t new_deq =
                rt_atomic_load_explicit(&queue->deq, memory_order_relaxed);
            if (new_deq != last_deq)
            {
                deq = new_deq;
                last_deq = new_deq;
            }
            else
            {
                deq = next(deq, queue->num_elems);
            }
        }

        unsigned char recv_s = sgen(s) | SLOT_RECV;
        if (rt_atomic_compare_exchange_strong_explicit(slot, &s, recv_s,
                                                       memory_order_acquire,
                                                       memory_order_relaxed))
        {
            rt_logf("peek: slot %zu claimed...\n", qindex(deq));

            const unsigned char *const p = queue->data;
            memcpy(elem, &p[queue->elem_size * qindex(deq)], queue->elem_size);

            const unsigned char full_s =
                (sgen(s) + SLOT_GEN_INCREMENT) | SLOT_FULL;
            if (rt_atomic_compare_exchange_strong_explicit(
                    slot, &recv_s, full_s, memory_order_relaxed,
                    memory_order_relaxed))
            {
                break;
            }
        }
    }
    /* After peeking, another receiver may run. */
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
