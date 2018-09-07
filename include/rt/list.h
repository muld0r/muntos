#pragma once

#include "container.h"

struct list
{
  struct list *prev, *next;
};

#define LIST_INIT(name)                                                        \
  {                                                                            \
    .prev = &(name), .next = &(name),                                          \
  }

#define LIST_HEAD(name) struct list name = LIST_INIT(name)

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

static inline struct list *list_front(struct list *head)
{
  return head->next;
}

#define list_for_each(node, head)                                              \
  for (struct list *node = (head)->next; node != (head); node = node->next)

#define list_for_each_item(item, head, type, member)                           \
  for (type *item = list_item(list_front(head), type, member);                 \
       &item->member != (head);                                                \
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
