#ifndef RT_PQ_H
#define RT_PQ_H

#include <rt/list.h>

#include <stdbool.h>
#include <stddef.h>

struct rt_pq
{
    struct rt_list list;
    size_t size;
    bool (*less_than)(const struct rt_list *, const struct rt_list *);
};

void rt_pq_init(struct rt_pq *pq, bool (*less_than)(const struct rt_list *,
                                                    const struct rt_list *));

void rt_pq_insert(struct rt_pq *pq, struct rt_list *node);

struct rt_list *rt_pq_min(struct rt_pq *pq);

void rt_pq_remove(struct rt_pq *pq, struct rt_list *node);

struct rt_list *rt_pq_pop_min(struct rt_pq *pq);

size_t rt_pq_size(const struct rt_pq *pq);

bool rt_pq_is_empty(const struct rt_pq *pq);

#define RT_PQ_INIT(name, less_than_)                                           \
    {                                                                          \
        .list = RT_LIST_INIT(name.list), .size = 0, .less_than = less_than_,   \
    }

#define RT_PQ(name, less_than) struct rt_pq name = RT_PQ_INIT(name, less_than)

#endif /* RT_PQ_H */
