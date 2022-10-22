#ifndef RT_SBHEAP_H
#define RT_SBHEAP_H

#include <rt/list.h>

#include <stdbool.h>
#include <stddef.h>

struct rt_sbheap_node
{
    struct rt_list list, children, singletons;
    size_t order;
};

struct rt_sbheap
{
    struct rt_list trees;
    size_t num_nodes, num_trees;
    bool (*less_than)(const struct rt_sbheap_node *,
                      const struct rt_sbheap_node *);
};

void rt_sbheap_init(struct rt_sbheap *heap,
                    bool (*less_than)(const struct rt_sbheap_node *,
                                      const struct rt_sbheap_node *));

void rt_sbheap_insert(struct rt_sbheap *heap, struct rt_sbheap_node *node);

struct rt_sbheap_node *rt_sbheap_pop_min(struct rt_sbheap *heap);

void rt_sbheap_merge(struct rt_sbheap *heap, struct rt_sbheap *other);

size_t rt_sbheap_size(const struct rt_sbheap *heap);

bool rt_sbheap_is_empty(const struct rt_sbheap *heap);

#endif /* RT_SBHEAP_H */
