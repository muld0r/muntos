#include <rt/atomic.h>
#include <rt/queue.h>

#include <rt/log.h>
#include <rt/task.h>

#include <stdatomic.h>
#include <string.h>

/* An empty slot, ready to receive new data. */
#define SLOT_EMPTY 0x0U

/* An empty slot that has been claimed by a sender. */
#define SLOT_SEND 0x01U

/* A full slot that has been claimed by a receiver/peeker. */
#define SLOT_RECV 0x02U

/* A full slot ready to be received. */
#define SLOT_FULL 0x03U

#define SLOT_STATE_MASK 0x03U

#define SLOT_GEN_INCREMENT 0x04U
#define SLOT_GEN_MASK (0xFFU & ~SLOT_STATE_MASK)

static inline unsigned char state(unsigned char slot)
{
    return slot & SLOT_STATE_MASK;
}

static inline unsigned char gen(unsigned char slot)
{
    return slot & SLOT_GEN_MASK;
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

static size_t next(size_t i, size_t num_elems)
{
    i += 1;
    if (i == num_elems)
    {
        return 0;
    }
    return i;
}

static void send(struct rt_queue *queue, const void *elem)
{
    size_t enq = rt_atomic_load_explicit(&queue->enq, memory_order_relaxed);
    for (;;)
    {
        size_t next_enq = next(enq, queue->num_elems);

        /* Load the slot and attempt to claim it if it's empty. */
        unsigned char slot =
            rt_atomic_load_explicit(&queue->slots[enq], memory_order_relaxed);
        const unsigned char send_slot = gen(slot) | SLOT_SEND;
        rt_logf("send: slot %zu %s\n", enq, state_str(state(slot)));
        if ((state(slot) == SLOT_EMPTY) &&
            rt_atomic_compare_exchange_strong_explicit(&queue->slots[enq],
                                                       &slot, send_slot,
                                                       memory_order_relaxed,
                                                       memory_order_relaxed))
        {
            rt_logf("send: slot %zu claimed...\n", enq);
            rt_atomic_compare_exchange_strong_explicit(&queue->enq, &enq,
                                                       next_enq,
                                                       memory_order_relaxed,
                                                       memory_order_relaxed);

            slot = send_slot;
            unsigned char *const p = queue->data;
            memcpy(&p[queue->elem_size * enq], elem, queue->elem_size);
            if (rt_atomic_compare_exchange_strong_explicit(
                    &queue->slots[enq], &slot, gen(slot) | SLOT_FULL,
                    memory_order_release, memory_order_release))
            /* release even on failure so that the memcpy is complete before we
             * release the slot */
            {
                break;
            }
            rt_logf("send: slot %zu skipped...\n", enq);
            /* If our slot has been skipped by a reader, then restore it
             * back to empty and keep looking. */
            rt_atomic_store_explicit(&queue->slots[enq], gen(slot) | SLOT_EMPTY,
                                     memory_order_relaxed);
        }
        enq = next_enq;
    }
    rt_sem_post(&queue->recv_sem);
}

static void recv(struct rt_queue *queue, void *elem)
{
    size_t deq = rt_atomic_load_explicit(&queue->deq, memory_order_relaxed);
    for (;;)
    {
        size_t next_deq = next(deq, queue->num_elems);

        unsigned char slot =
            rt_atomic_load_explicit(&queue->slots[deq], memory_order_relaxed);
        rt_logf("recv: slot %zu %s\n", deq, state_str(state(slot)));
        if (state(slot) == SLOT_SEND)
        {
            const unsigned char skipped_slot =
                (gen(slot) + SLOT_GEN_INCREMENT) | SLOT_SEND;
            /* If we encounter an in-progress send, attempt to skip it by
             * incrementing its generation. The send may have completed in the
             * mean time, in which case we can attempt to read from the slot.
             * If not, there will be full slots to receive after this slot,
             * because the level allowing receivers to run is only incremented
             * after each send is complete. */
            rt_atomic_compare_exchange_strong_explicit(&queue->slots[deq],
                                                       &slot, skipped_slot,
                                                       memory_order_relaxed,
                                                       memory_order_relaxed);
            continue;
        }

        if ((state(slot) == SLOT_FULL) &&
            rt_atomic_compare_exchange_strong_explicit(&queue->slots[deq],
                                                       &slot,
                                                       gen(slot) | SLOT_RECV,
                                                       memory_order_acquire,
                                                       memory_order_relaxed))
        {
            rt_logf("recv: slot %zu claimed...\n", deq);
            const unsigned char *const p = queue->data;
            memcpy(elem, &p[queue->elem_size * deq], queue->elem_size);
            const unsigned char empty_slot =
                (gen(slot) + SLOT_GEN_INCREMENT) | SLOT_EMPTY;
            rt_atomic_store_explicit(&queue->slots[deq], empty_slot,
                                     memory_order_relaxed);
            rt_atomic_compare_exchange_strong_explicit(&queue->deq, &deq,
                                                       next_deq,
                                                       memory_order_relaxed,
                                                       memory_order_relaxed);
            break;
        }
        deq = next_deq;
    }
    rt_sem_post(&queue->send_sem);
}

static void peek(struct rt_queue *queue, void *elem)
{
    /* Similar to recv, but don't update the dequeue index and set the slot
     * back to SLOT_FULL after reading it so another receiver can read it. */
    size_t deq = rt_atomic_load_explicit(&queue->deq, memory_order_relaxed);
    for (;;)
    {
        unsigned char slot =
            rt_atomic_load_explicit(&queue->slots[deq], memory_order_relaxed);
        if ((state(slot) == SLOT_FULL) &&
            rt_atomic_compare_exchange_strong_explicit(&queue->slots[deq],
                                                       &slot,
                                                       gen(slot) | SLOT_RECV,
                                                       memory_order_acquire,
                                                       memory_order_relaxed))
        {
            const unsigned char *const p = queue->data;
            memcpy(elem, &p[queue->elem_size * deq], queue->elem_size);
            rt_atomic_store_explicit(&queue->slots[deq], gen(slot) | SLOT_FULL,
                                     memory_order_relaxed);
            break;
        }
        deq = next(deq, queue->num_elems);
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
