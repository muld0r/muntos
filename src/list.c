#include <rt/list.h>

void rt_list_init(struct rt_list *list)
{
    list->prev = list;
    list->next = list;
}

void rt_list_insert_before(struct rt_list *node, struct rt_list *next)
{
    struct rt_list *const prev = next->prev;
    node->next = next;
    node->prev = prev;
    prev->next = node;
    next->prev = node;
}

void rt_list_push_front(struct rt_list *list, struct rt_list *node)
{
    rt_list_insert_before(node, list->next);
}

void rt_list_push_back(struct rt_list *list, struct rt_list *node)
{
    rt_list_insert_before(node, list);
}

void rt_list_remove(struct rt_list *node)
{
    struct rt_list *const next = node->next, *const prev = node->prev;
    next->prev = prev;
    prev->next = next;
    node->prev = node;
    node->next = node;
}

bool rt_list_is_empty(struct rt_list *list)
{
    return list->next == list;
}

struct rt_list *rt_list_front(struct rt_list *list)
{
    return list->next;
}

struct rt_list *rt_list_pop_front(struct rt_list *list)
{
    struct rt_list *const front = rt_list_front(list);
    rt_list_remove(front);
    return front;
}
