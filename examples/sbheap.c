#include <rt/container.h>
#include <rt/list.h>
#include <rt/log.h>
#include <rt/sbheap.h>

struct u_node
{
    unsigned x;
    struct rt_sbheap_node node;
};

#define NUM_NODES 20

static struct u_node nodes[NUM_NODES];

static bool u_node_less_than(const struct rt_sbheap_node *a,
                             const struct rt_sbheap_node *b)
{
    return rt_container_of(a, struct u_node, node)->x <
           rt_container_of(b, struct u_node, node)->x;
}

static struct u_node *heap_item(const struct rt_sbheap_node *node)
{
    return rt_container_of(node, struct u_node, node);
}

static struct u_node *list_item(const struct rt_list *node)
{
    return heap_item(rt_sbheap_from_list(node));
}

static void print_indent(size_t indent)
{
    if (indent > 0)
    {
        for (size_t i = 0; i < indent - 1; ++i)
        {
            rt_logf("    ");
        }
        rt_logf("└───");
    }
}

static void print_tree(const struct rt_sbheap_node *tree, size_t indent)
{
    print_indent(indent);
    rt_logf("%u\n", heap_item(tree)->x);

    struct rt_list *node;
    rt_list_for_each(node, &tree->singletons)
    {
        print_indent(indent);
        rt_logf("%u\n", list_item(node)->x);
    }

    if (!rt_list_is_empty(&tree->children))
    {
        rt_list_for_each(node, &tree->children)
        {
            print_tree(rt_sbheap_from_list(node), indent + 1);
        }
    }
}

static void print_heap(const struct rt_sbheap *heap)
{
    rt_logf("\nheap size %zu\n", rt_sbheap_size(heap));
    struct rt_list *node;
    rt_list_for_each(node, &heap->trees)
    {
        print_tree(rt_sbheap_from_list(node), 0);
    }
}

int main(void)
{
    struct rt_sbheap heap;
    rt_sbheap_init(&heap, u_node_less_than);
    uint32_t seed = 0;
    uint32_t a = 1664525, c = 1013904223;
    int i;
    for (i = 0; i < NUM_NODES; ++i)
    {
        seed = ((a * seed) + c);
        nodes[i].x = seed % 1000;
        rt_sbheap_insert(&heap, &nodes[i].node);
        print_heap(&heap);
    }

    /* Check that the output is in sorted order. */
    uint32_t max = 0;
    while (!rt_sbheap_is_empty(&heap))
    {
        print_heap(&heap);
        uint32_t x = heap_item(rt_sbheap_pop_min(&heap))->x;
        if (x < max)
        {
            return 1;
        }
        max = x;
    }
}
