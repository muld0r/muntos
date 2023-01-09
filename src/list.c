#include <rt/list.h>

void rt_list_init(struct rt_list *list)
{
    list->prev = list;
    list->next = list;
}

static void insert(struct rt_list *node, struct rt_list *prev,
                   struct rt_list *next)
{
    next->prev = node;
    prev->next = node;
    node->prev = prev;
    node->next = next;
}

void rt_list_insert_before(struct rt_list *node, struct rt_list *next)
{
    insert(node, next->prev, next);
}

void rt_list_push_front(struct rt_list *list, struct rt_list *node)
{
    insert(node, list, list->next);
}

void rt_list_push_back(struct rt_list *list, struct rt_list *node)
{
    insert(node, list->prev, list);
}

void rt_list_remove(struct rt_list *node)
{
    struct rt_list *const next = node->next, *const prev = node->prev;
    node->prev = node;
    node->next = node;
    next->prev = prev;
    prev->next = next;
}

bool rt_list_is_empty(const struct rt_list *list)
{
    return list->next == list;
}

struct rt_list *rt_list_front(const struct rt_list *list)
{
    return list->next;
}

struct rt_list *rt_list_pop_front(struct rt_list *list)
{
    struct rt_list *const front = rt_list_front(list);
    rt_list_remove(front);
    return front;
}

void rt_list_insert_by(struct rt_list *list, struct rt_list *node,
                       bool (*less_than)(const struct rt_list *a,
                                         const struct rt_list *b))
{
    struct rt_list *successor;
    rt_list_for_each(successor, list)
    {
        if (less_than(node, successor))
        {
            break;
        }
    }
    rt_list_insert_before(node, successor);
}

void rt_list_move_all(struct rt_list *dst, struct rt_list *src)
{
    rt_list_insert_before(dst, src);
    rt_list_remove(src);
}
