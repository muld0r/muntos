#ifndef RT_LIST_H
#define RT_LIST_H

#include <stdbool.h>

struct rt_list
{
    struct rt_list *prev, *next;
};

void rt_list_init(struct rt_list *list);

void rt_list_insert_before(struct rt_list *node, struct rt_list *next);

void rt_list_push_front(struct rt_list *list, struct rt_list *node);

void rt_list_push_back(struct rt_list *list, struct rt_list *node);

void rt_list_remove(struct rt_list *node);

bool rt_list_is_empty(const struct rt_list *list);

struct rt_list *rt_list_front(struct rt_list *list);

struct rt_list *rt_list_pop_front(struct rt_list *list);

#define rt_list_for_each(node, listp)                                          \
    for (node = rt_list_front((listp)); node != (listp); node = node->next)

#define RT_LIST_INIT(name)                                                     \
    {                                                                          \
        .prev = &name, .next = &name                                           \
    }

#define RT_LIST(name) struct rt_list name = RT_LIST_INIT(name)

#endif /* RT_LIST_H */
