#include <rt/pq.h>

void rt_pq_init(struct rt_pq *pq, bool (*less_than)(const struct rt_list *,
                                                    const struct rt_list *))
{
    rt_list_init(&pq->list);
    pq->size = 0;
    pq->less_than = less_than;
}

void rt_pq_insert(struct rt_pq *pq, struct rt_list *node)
{
    struct rt_list *successor;
    rt_list_for_each(successor, &pq->list)
    {
        if (pq->less_than(node, successor))
        {
            break;
        }
    }
    rt_list_insert_before(node, successor);
    ++pq->size;
}

struct rt_list *rt_pq_min(struct rt_pq *pq)
{
    if (pq->size == 0)
    {
        return NULL;
    }
    return rt_list_front(&pq->list);
}

void rt_pq_remove(struct rt_pq *pq, struct rt_list *node)
{
    --pq->size;
    rt_list_remove(node);
}

struct rt_list *rt_pq_pop_min(struct rt_pq *pq)
{
    if (pq->size == 0)
    {
        return NULL;
    }
    --pq->size;
    return rt_list_pop_front(&pq->list);
}

size_t rt_pq_size(const struct rt_pq *pq)
{
    return pq->size;
}

bool rt_pq_is_empty(const struct rt_pq *pq)
{
    return pq->size == 0;
}
