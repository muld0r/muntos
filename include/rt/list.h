#pragma once

#include "container.h"

#include <stdbool.h>

struct list
{
    struct list *prev, *next;
};

#define LIST_HEAD_INIT(name)                                                   \
    {                                                                          \
        .prev = &(name), .next = &(name),                                      \
    }

#define LIST_NODE_INIT                                                         \
    {                                                                          \
        .prev = NULL, .next = NULL,                                            \
    }

#define LIST_HEAD(name) struct list name = LIST_HEAD_INIT(name)

static inline void list_head_init(struct list *head)
{
    head->prev = head;
    head->next = head;
}

static inline void list_node_init(struct list *node)
{
    node->prev = NULL;
    node->next = NULL;
}

#define list_item container_item

static inline void list_insert(struct list *node, struct list *prev,
                               struct list *next)
{
    next->prev = node;
    node->next = next;
    node->prev = prev;
    prev->next = node;
}

static inline void list_add(struct list *head, struct list *node)
{
    list_insert(node, head, head->next);
}

static inline void list_add_tail(struct list *head, struct list *node)
{
    list_insert(node, head->prev, head);
}

static inline void list_remove(struct list *node)
{
    if (node->next && node->prev)
    {
        node->next->prev = node->prev;
        node->prev->next = node->next;
        node->next = node->prev = NULL;
    }
}

static inline bool list_empty(struct list *head)
{
    return head == head->next;
}

static inline struct list *list_front(struct list *head)
{
    if (!list_empty(head))
    {
        return head->next;
    }
    else
    {
        return NULL;
    }
}

#define list_for_each(node, head)                                              \
    for (struct list *node = (head)->next; node != (head); node = node->next)

#define list_for_each_item(item, head, type, member)                           \
    for (type *item = list_item(list_front(head), type, member);               \
         &item->member != (head);                                              \
         item = list_item(item->member.next, type, member))

static inline size_t list_len(struct list *head)
{
    size_t i = 0;
    list_for_each(node, head)
    {
        ++i;
    }
    return i;
}
