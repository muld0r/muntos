#include <rt/list.h>

#include <stdlib.h>
#include <stdio.h>

struct intl
{
    int x;
    struct list list;
};

static struct intl *new_int(int x)
{
    struct intl *intl = malloc(sizeof *intl);
    intl->x = x;
    list_init(&intl->list);
    return intl;
}

int main(void)
{
    struct list list;
    list_init(&list);
    for (int i = 0; i < 5; ++i)
    {
        list_push_front(&list, &new_int(i)->list);
    }

    struct list *node;
    list_for_each(node, &list)
    {
        printf("%d\n", list_item(node, struct intl, list)->x);
    }

    while (!list_is_empty(&list))
    {
        free(list_item(list_pop_front(&list), struct intl, list));
    }
}
