#include <rt/queue.h>

#include <rt/syscall.h>

#include <string.h>

#define SLOT_EMPTY '\0' /* An empty slot, ready to receive new data. */
#define SLOT_RECV 'r'   /* A full slot that has been claimed by a receiver. */
#define SLOT_SEND 's'   /* An empty slot that has been claimed by a sender. */
#define SLOT_FULL 'f'   /* A full slot ready to be received. */

static void send(struct rt_queue *queue, const void *elem)
{
    size_t enq = rt_atomic_load_explicit(&queue->enq, memory_order_relaxed);
    size_t start_enq = enq;
    for (;;)
    {
        size_t next_enq = enq + 1;
        if (next_enq == queue->num_elems)
        {
            next_enq = 0;
        }
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
            rt_atomic_store_explicit(&queue->slots[enq], SLOT_FULL,
                                     memory_order_release);
            /* Update enqueue index if no one has yet. */
            rt_atomic_compare_exchange_strong_explicit(&queue->enq, &start_enq,
                                                       next_enq,
                                                       memory_order_relaxed,
                                                       memory_order_relaxed);
            return;
        }
        enq = next_enq;
    }
}

static void recv(struct rt_queue *queue, void *elem)
{
    size_t deq = rt_atomic_load_explicit(&queue->deq, memory_order_relaxed);
    size_t start_deq = deq;
    for (;;)
    {
        size_t next_deq = deq + 1;
        if (next_deq == queue->num_elems)
        {
            next_deq = 0;
        }
        char slot =
            rt_atomic_load_explicit(&queue->slots[deq], memory_order_relaxed);
        if ((slot == SLOT_FULL) &&
            rt_atomic_compare_exchange_strong_explicit(&queue->slots[deq],
                                                       &slot, SLOT_RECV,
                                                       memory_order_acquire,
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
            return;
        }
        deq = next_deq;
    }
}

static void wake(struct rt_queue *queue)
{
    if (!rt_atomic_flag_test_and_set_explicit(&queue->wake_pending,
                                              memory_order_acquire))
    {
        rt_syscall(&queue->wake_record);
    }
}

void rt_queue_send(struct rt_queue *queue, const void *elem)
{
    const long level =
        rt_atomic_fetch_add_explicit(&queue->level, 1, memory_order_relaxed);
    if (level >= (long)queue->num_elems)
    {
        /* Queue is full, syscall. */
        struct rt_syscall_record syscall_record = {
            .syscall = RT_SYSCALL_QUEUE_SEND,
            .args.queue = queue,
        };
        rt_syscall(&syscall_record);
    }
    send(queue, elem);
    if (level < 0)
    {
        wake(queue);
    }
}

void rt_queue_recv(struct rt_queue *queue, void *elem)
{
    const long level =
        rt_atomic_fetch_sub_explicit(&queue->level, 1, memory_order_relaxed);
    if (level <= 0)
    {
        /* Queue is empty, syscall. */
        struct rt_syscall_record syscall_record = {
            .syscall = RT_SYSCALL_QUEUE_RECV,
            .args.queue = queue,
        };
        rt_syscall(&syscall_record);
    }
    recv(queue, elem);
    if (level > (long)queue->num_elems)
    {
        wake(queue);
    }
}

bool rt_queue_trysend(struct rt_queue *queue, const void *elem)
{
    long level = rt_atomic_load_explicit(&queue->level, memory_order_relaxed);
    do
    {
        if (level >= (long)queue->num_elems)
        {
            /* Queue is full. */
            return false;
        }
    } while (!rt_atomic_compare_exchange_weak_explicit(&queue->level, &level,
                                                       level + 1,
                                                       memory_order_relaxed,
                                                       memory_order_relaxed));
    send(queue, elem);
    if (level < 0)
    {
        wake(queue);
    }
    return true;
}

bool rt_queue_tryrecv(struct rt_queue *queue, void *elem)
{
    long level = rt_atomic_load_explicit(&queue->level, memory_order_relaxed);
    do
    {
        if (level <= 0)
        {
            /* Queue is empty. */
            return false;
        }
    } while (!rt_atomic_compare_exchange_weak_explicit(&queue->level, &level,
                                                       level - 1,
                                                       memory_order_relaxed,
                                                       memory_order_relaxed));
    recv(queue, elem);
    if (level > (long)queue->num_elems)
    {
        wake(queue);
    }
    return true;
}
