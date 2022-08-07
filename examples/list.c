#include <rt/list.h>

#include <stdio.h>
#include <stdlib.h>

struct intl
{
    int x;
    struct rt_list list;
};

static struct intl *new_int(int x)
{
    struct intl *intl = malloc(sizeof *intl);
    intl->x = x;
    rt_list_init(&intl->list);
    return intl;
}

int main(void)
{
    struct rt_list list;
    rt_list_init(&list);
    for (int i = 0; i < 5; ++i)
    {
        rt_list_push_front(&list, &new_int(i)->list);
    }

    struct rt_list *node;
    rt_list_for_each(node, &list)
    {
        printf("%d\n", rt_list_item(node, struct intl, list)->x);
    }

    while (!rt_list_is_empty(&list))
    {
        free(rt_list_item(rt_list_pop_front(&list), struct intl, list));
    }
}
