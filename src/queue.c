#include <rt/atomic.h>
#include <rt/queue.h>

#include <rt/log.h>
#include <rt/task.h>

#include <limits.h>
#include <stdatomic.h>
#include <string.h>

/* An empty slot ready to be pushed. */
#define SLOT_EMPTY 0x00U

/* An empty slot that has been claimed by a pusher. */
#define SLOT_PUSH 0x05U

/* A full slot that has been claimed by a popper/peeker. */
#define SLOT_POP 0x0AU

/* An empty slot that has been skipped by a popper. */
#define SLOT_SKIPPED 0x0CU

/* A full slot ready to be popped. */
#define SLOT_FULL 0x0FU

#define SLOT_STATE_MASK 0x0FU

#define SLOT_GEN_INCREMENT (1U << RT_QUEUE_STATE_BITS)
#define SLOT_GEN_MASK (UCHAR_MAX & ~SLOT_STATE_MASK)

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
    case SLOT_PUSH:
        return "push";
    case SLOT_POP:
        return "pop";
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

static void push(struct rt_queue *queue, const void *elem)
{
    for (;;)
    {
        size_t enq = rt_atomic_load_explicit(&queue->enq, memory_order_relaxed);
        size_t last_enq = enq;
        rt_atomic_uchar *slot;
        unsigned char s;
        for (;;)
        {
            slot = &queue->slots[qindex(enq)];
            s = rt_atomic_load_explicit(slot, memory_order_relaxed);
            rt_logf("push: slot %zu %s\n", qindex(enq), state_str(state(s)));
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

        const unsigned char push_s = sgen(s) | SLOT_PUSH;
        if (rt_atomic_compare_exchange_strong_explicit(slot, &s, push_s,
                                                       memory_order_relaxed,
                                                       memory_order_relaxed))
        {
            rt_logf("push: slot %zu claimed...\n", qindex(enq));
            rt_atomic_store_explicit(&queue->enq, next(enq, queue->num_elems),
                                     memory_order_relaxed);

            unsigned char *const p = queue->data;
            memcpy(&p[queue->elem_size * qindex(enq)], elem, queue->elem_size);

            s = push_s;
            if (rt_atomic_compare_exchange_strong_explicit(
                    slot, &s, sgen(s) | SLOT_FULL, memory_order_release,
                    memory_order_relaxed))
            {
                break;
            }

            rt_logf("push: slot %zu skipped...\n", qindex(enq));
            /* If our slot has been skipped by a reader, then restore it
             * back to empty and keep looking. */
            while (!rt_atomic_compare_exchange_weak_explicit(
                slot, &s, sgen(s) | SLOT_EMPTY, memory_order_release,
                memory_order_relaxed))
            {
            }
        }
    }
    rt_sem_post(&queue->pop_sem);
}

static void pop(struct rt_queue *queue, void *elem)
{
    for (;;)
    {
        size_t deq = rt_atomic_load_explicit(&queue->deq, memory_order_relaxed);
        size_t last_deq = deq;
        rt_atomic_uchar *slot;
        unsigned char s;
        for (;;)
        {
            slot = &queue->slots[qindex(deq)];
            s = rt_atomic_load_explicit(slot, memory_order_relaxed);
            rt_logf("pop: slot %zu %s\n", qindex(deq), state_str(state(s)));
            if (sgen(s) == qsgen(deq))
            {
                if ((state(s) == SLOT_PUSH) || (state(s) == SLOT_SKIPPED))
                {
                    const unsigned char skipped_slot =
                        (sgen(s) + SLOT_GEN_INCREMENT) | SLOT_SKIPPED;
                    /* If we encounter an in-progress push, attempt to skip it.
                     * The push may have completed in the mean time, in which
                     * case we can attempt to pop from the slot. If not, there
                     * will be full slots to pop after this slot, because
                     * the level allowing poppers to run is only incremented
                     * after each push is complete. */
                    if (rt_atomic_compare_exchange_strong_explicit(
                            slot, &s, skipped_slot, memory_order_relaxed,
                            memory_order_relaxed))
                    {
                        rt_logf("pop: slot %zu skipped...\n", qindex(deq));
                    }
                }
                if ((state(s) == SLOT_FULL) || (state(s) == SLOT_POP))
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

        unsigned char pop_s = sgen(s) | SLOT_POP;
        if (rt_atomic_compare_exchange_strong_explicit(slot, &s, pop_s,
                                                       memory_order_acquire,
                                                       memory_order_relaxed))
        {
            rt_logf("pop: slot %zu claimed...\n", qindex(deq));

            const unsigned char *const p = queue->data;
            memcpy(elem, &p[queue->elem_size * qindex(deq)], queue->elem_size);

            const unsigned char empty_s =
                (sgen(s) + SLOT_GEN_INCREMENT) | SLOT_EMPTY;
            if (rt_atomic_compare_exchange_strong_explicit(
                    slot, &pop_s, empty_s, memory_order_relaxed,
                    memory_order_relaxed))
            {
                rt_atomic_store_explicit(&queue->deq,
                                         next(deq, queue->num_elems),
                                         memory_order_relaxed);
                break;
            }
        }
    }
    rt_sem_post(&queue->push_sem);
}

static void peek(struct rt_queue *queue, void *elem)
{
    /* Similar to pop, but don't update the dequeue index and set the slot back
     * to SLOT_FULL after reading it so another popper/peeker can read it. */
    for (;;)
    {
        size_t deq = rt_atomic_load_explicit(&queue->deq, memory_order_relaxed);
        size_t last_deq = deq;
        rt_atomic_uchar *slot;
        unsigned char s;
        for (;;)
        {
            slot = &queue->slots[qindex(deq)];
            s = rt_atomic_load_explicit(slot, memory_order_relaxed);
            rt_logf("peek: slot %zu %s\n", qindex(deq), state_str(state(s)));
            if (sgen(s) == qsgen(deq))
            {
                if ((state(s) == SLOT_FULL) || (state(s) == SLOT_POP))
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

        unsigned char pop_s = sgen(s) | SLOT_POP;
        if (rt_atomic_compare_exchange_strong_explicit(slot, &s, pop_s,
                                                       memory_order_acquire,
                                                       memory_order_relaxed))
        {
            rt_logf("peek: slot %zu claimed...\n", qindex(deq));

            const unsigned char *const p = queue->data;
            memcpy(elem, &p[queue->elem_size * qindex(deq)], queue->elem_size);

            const unsigned char full_s =
                (sgen(s) + SLOT_GEN_INCREMENT) | SLOT_FULL;
            if (rt_atomic_compare_exchange_strong_explicit(slot, &pop_s, full_s,
                                                           memory_order_relaxed,
                                                           memory_order_relaxed))
            {
                break;
            }
        }
    }
    /* After peeking, another popper/peeker may run. */
    rt_sem_post(&queue->pop_sem);
}

void rt_queue_push(struct rt_queue *queue, const void *elem)
{
    rt_sem_wait(&queue->push_sem);
    push(queue, elem);
}

void rt_queue_pop(struct rt_queue *queue, void *elem)
{
    rt_sem_wait(&queue->pop_sem);
    pop(queue, elem);
}

void rt_queue_peek(struct rt_queue *queue, void *elem)
{
    rt_sem_wait(&queue->pop_sem);
    peek(queue, elem);
}

bool rt_queue_trypush(struct rt_queue *queue, const void *elem)
{
    if (!rt_sem_trywait(&queue->push_sem))
    {
        return false;
    }
    push(queue, elem);
    return true;
}

bool rt_queue_trypop(struct rt_queue *queue, void *elem)
{
    if (!rt_sem_trywait(&queue->pop_sem))
    {
        return false;
    }
    pop(queue, elem);
    return true;
}

bool rt_queue_trypeek(struct rt_queue *queue, void *elem)
{
    if (!rt_sem_trywait(&queue->pop_sem))
    {
        return false;
    }
    peek(queue, elem);
    return true;
}

bool rt_queue_timedpush(struct rt_queue *queue, const void *elem,
                        unsigned long ticks)
{
    if (!rt_sem_timedwait(&queue->push_sem, ticks))
    {
        return false;
    }
    push(queue, elem);
    return true;
}

bool rt_queue_timedpop(struct rt_queue *queue, void *elem, unsigned long ticks)
{
    if (!rt_sem_timedwait(&queue->pop_sem, ticks))
    {
        return false;
    }
    pop(queue, elem);
    return true;
}

bool rt_queue_timedpeek(struct rt_queue *queue, void *elem, unsigned long ticks)
{
    if (!rt_sem_timedwait(&queue->pop_sem, ticks))
    {
        return false;
    }
    peek(queue, elem);
    return true;
}
