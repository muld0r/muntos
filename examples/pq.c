#include <rt/container.h>
#include <rt/list.h>
#include <rt/log.h>
#include <rt/pq.h>

struct u_node
{
    struct rt_list list;
    unsigned x;
};

#define NUM_NODES 20

static struct u_node nodes[NUM_NODES];

static bool u_node_less_than(const struct rt_list *a,
                             const struct rt_list *b)
{
    return rt_container_of(a, struct u_node, list)->x <
           rt_container_of(b, struct u_node, list)->x;
}

static struct u_node *list_item(const struct rt_list *node)
{
    return rt_container_of(node, struct u_node, list);
}

static void print_pq(const struct rt_pq *pq)
{
    rt_logf("\npq size %zu\n", rt_pq_size(pq));
    struct rt_list *node;
    rt_list_for_each(node, &pq->list)
    {
        rt_logf("%u\n", list_item(node)->x);
    }
}

int main(void)
{
    RT_PQ(pq, u_node_less_than);
    uint32_t seed = 0;
    uint32_t a = 1664525, c = 1013904223;
    int i;
    for (i = 0; i < NUM_NODES; ++i)
    {
        seed = ((a * seed) + c);
        nodes[i].x = seed % 1000;
        rt_pq_insert(&pq, &nodes[i].list);
        print_pq(&pq);
    }

    /* Check that the output is in sorted order. */
    uint32_t max = 0;
    while (!rt_pq_is_empty(&pq))
    {
        print_pq(&pq);
        uint32_t x = list_item(rt_pq_pop_min(&pq))->x;
        if (x < max)
        {
            return 1;
        }
        max = x;
    }
}
