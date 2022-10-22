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

struct rt_sbheap_node *rt_sbheap_min(const struct rt_sbheap *heap);

void rt_sbheap_remove(struct rt_sbheap *heap, struct rt_sbheap_node *node);

struct rt_sbheap_node *rt_sbheap_pop_min(struct rt_sbheap *heap);

void rt_sbheap_merge(struct rt_sbheap *heap, struct rt_sbheap *other);

size_t rt_sbheap_size(const struct rt_sbheap *heap);

bool rt_sbheap_is_empty(const struct rt_sbheap *heap);

bool rt_sbheap_node_in_heap(const struct rt_sbheap_node *node);

#define RT_SBHEAP_NODE_INIT(name)                                              \
    {                                                                          \
        .list = RT_LIST_INIT(name.list),                                       \
        .children = RT_LIST_INIT(name.children),                               \
        .singletons = RT_LIST_INIT(name.singletons), .order = 0,               \
    }

#define RT_SBHEAP_INIT(name, less_than_)                                       \
    {                                                                          \
        .trees = RT_LIST_INIT(name.trees), .num_nodes = 0, .num_trees = 0,     \
        .less_than = less_than_,                                               \
    }

#define RT_SBHEAP(name, less_than)                                             \
    struct rt_sbheap name = RT_SBHEAP_INIT(name, less_than)

#endif /* RT_SBHEAP_H */
