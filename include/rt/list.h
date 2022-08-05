#pragma once

#include "container.h"

#include <stdbool.h>

struct list
{
    struct list *prev, *next;
};

static inline void list_init(struct list *list)
{
    list->prev = list;
    list->next = list;
}

#define LIST_INIT(name)                                                        \
    {                                                                          \
        .prev = &name, .next = &name                                           \
    }

#define LIST(name) struct list name = LIST_INIT(name)

#define list_item container_item

static inline void list_insert_before(struct list *node, struct list *next)
{
    node->next = next;
    node->prev = next->prev;
    next->prev->next = node;
    next->prev = node;
}

static inline void list_push_front(struct list *list, struct list *node)
{
    list_insert_before(node, list->next);
}

static inline void list_push_back(struct list *list, struct list *node)
{
    list_insert_before(node, list);
}

static inline void list_remove(struct list *node)
{
    struct list *const next = node->next, *const prev = node->prev;
    next->prev = prev;
    prev->next = next;
    node->prev = node;
    node->next = node;
}

static inline bool list_is_empty(struct list *list)
{
    return list->next == list;
}

static inline struct list *list_front(struct list *list)
{
    return list->next;
}

static inline struct list *list_pop_front(struct list *list)
{
    struct list *front = list_front(list);
    if (front)
    {
        list_remove(front);
    }
    return front;
}

#define list_for_each(node, listp)                                             \
    for (node = list_front((listp)); node != (listp); node = node->next)
