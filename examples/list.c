#include <muntos/container.h>
#include <muntos/list.h>

struct intl
{
    int x;
    struct rt_list list;
};

static struct intl *item(struct rt_list *node)
{
    return rt_container_of(node, struct intl, list);
}

#define NUM_NODES 5

static struct intl nodes[NUM_NODES];

int main(void)
{
    struct rt_list list;
    rt_list_init(&list);
    int i;
    for (i = 0; i < NUM_NODES; ++i)
    {
        nodes[i].x = i;
        rt_list_push_back(&list, &nodes[i].list);
    }

    struct rt_list *node;
    i = 0;
    rt_list_for_each(node, &list)
    {
        if (item(node)->x != i)
        {
            return 1;
        }
        ++i;
    }

    i = 0;
    while (!rt_list_is_empty(&list))
    {
        if (item(rt_list_pop_front(&list))->x != i)
        {
            return 1;
        }
        ++i;
    }
}
