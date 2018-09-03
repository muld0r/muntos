#pragma once

#include "container.h"

struct list
{
  struct list *prev, *next;
};

#define LIST_HEAD_INIT(name)                                                   \
  {                                                                            \
    .prev = &(name), .next = &(name),                                          \
  }

#define LIST_HEAD(name) struct list name = LIST_HEAD_INIT(name)

static inline void list_init(struct list *head)
{
  head->prev = head;
  head->next = head;
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

static inline void list_del(struct list *node)
{
  node->next->prev = node->prev;
  node->prev->next = node->next;
}
