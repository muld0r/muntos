#include <muntos/container.h>
#include <muntos/list.h>
#include <muntos/log.h>

struct u_node
{
    struct rt_list list;
    unsigned x;
};

#define NUM_NODES 20

static struct u_node nodes[NUM_NODES];

static bool u_node_less_than(const struct rt_list *a, const struct rt_list *b)
{
    return rt_container_of(a, struct u_node, list)->x <
           rt_container_of(b, struct u_node, list)->x;
}

static struct u_node *list_item(const struct rt_list *node)
{
    return rt_container_of(node, struct u_node, list);
}

static void print_list(const struct rt_list *list)
{
    rt_logf("\n");
    struct rt_list *node;
    rt_list_for_each(node, list)
    {
        rt_logf("%u\n", list_item(node)->x);
    }
}

int main(void)
{
    RT_LIST(list);
    uint32_t seed = 0;
    uint32_t a = 1664525, c = 1013904223;
    int i;
    for (i = 0; i < NUM_NODES; ++i)
    {
        seed = ((a * seed) + c);
        nodes[i].x = seed % 1000;
        rt_list_insert_by(&list, &nodes[i].list, u_node_less_than);
        print_list(&list);
    }

    /* Check that the output is in sorted order. */
    uint32_t max = 0;
    while (!rt_list_is_empty(&list))
    {
        print_list(&list);
        uint32_t x = list_item(rt_list_pop_front(&list))->x;
        if (x < max)
        {
            return 1;
        }
        max = x;
    }
}
