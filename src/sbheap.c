#include <rt/sbheap.h>

#include <rt/container.h>
#include <rt/list.h>

/*
 * Implementation derived from Keith Schwarz's Archive of Interesting Code.
 * https://www.keithschwarz.com/interesting/
 * https://www.keithschwarz.com/interesting/code/?dir=skew-binomial-heap
 */

void rt_sbheap_init(struct rt_sbheap *heap,
                    bool (*less_than)(const struct rt_sbheap_node *,
                                      const struct rt_sbheap_node *))
{
    rt_list_init(&heap->trees);
    heap->num_nodes = 0;
    heap->num_trees = 0;
    heap->less_than = less_than;
}

static struct rt_sbheap_node *tree(struct rt_list *node)
{
    return rt_container_of(node, struct rt_sbheap_node, list);
}

static struct rt_sbheap_node *merge_trees(struct rt_sbheap *heap,
                                          struct rt_sbheap_node *a,
                                          struct rt_sbheap_node *b)
{
    /* Merging two trees makes the tree with the smaller root the parent of the
     * other tree, then increases its order. */
    if (heap->less_than(b, a))
    {
        rt_list_push_back(&b->children, &a->list);
        ++b->order;
        return b;
    }

    rt_list_push_back(&a->children, &b->list);
    ++a->order;
    return a;
}

static struct rt_sbheap_node *merge_singleton(struct rt_sbheap *heap,
                                              struct rt_sbheap_node *tree,
                                              struct rt_sbheap_node *single)
{
    /* Add a singleton node to a tree. If the singleton is less than the root
     * of the tree, make that node the new root and add the old root as the
     * singleton. */
    if (heap->less_than(single, tree))
    {
        rt_list_move_all(&single->children, &tree->children);
        rt_list_move_all(&single->singletons, &tree->singletons);
        single->order = tree->order;
        tree->order = 0;
        rt_list_push_back(&single->singletons, &tree->list);
        return single;
    }

    rt_list_push_back(&tree->singletons, &single->list);
    return tree;
}

static void insert_singleton(struct rt_sbheap *heap,
                             struct rt_sbheap_node *node)
{
    node->order = 0;
    rt_list_init(&node->children);
    rt_list_init(&node->singletons);

    /* If the last two trees have the same order, merge them together into one
     * tree using a skew merge. */
    if ((heap->num_trees >= 2) &&
        (tree(rt_list_front(&heap->trees))->order ==
         tree(rt_list_front(&heap->trees)->next)->order))
    {
        /* Remove both trees from the tree list and merge them. */
        struct rt_sbheap_node *newtree =
            merge_trees(heap, tree(rt_list_pop_front(&heap->trees)),
                        tree(rt_list_pop_front(&heap->trees)));

        newtree = merge_singleton(heap, newtree, node);

        /* Finally, put this tree at the front of the list of trees. */
        rt_list_push_front(&heap->trees, &newtree->list);
        --heap->num_trees;
    }
    /* Otherwise, just prepend the tree to the front of the list; we have
     * nothing to merge. */
    else
    {
        rt_list_push_front(&heap->trees, &node->list);
        ++heap->num_trees;
    }
}

void rt_sbheap_insert(struct rt_sbheap *heap, struct rt_sbheap_node *node)
{
    insert_singleton(heap, node);
    ++heap->num_nodes;
}

static void merge_heaps(struct rt_sbheap *heap, struct rt_list *other_trees)
{
    /* Merge all of the trees from the two input sequences together by their
     * length. */
    RT_LIST(all_trees);
    while (!rt_list_is_empty(&heap->trees) && !rt_list_is_empty(other_trees))
    {
        if (tree(rt_list_front(&heap->trees))->order <
            tree(rt_list_front(other_trees))->order)
        {
            rt_list_push_back(&all_trees, rt_list_pop_front(&heap->trees));
        }
        else
        {
            rt_list_push_back(&all_trees, rt_list_pop_front(other_trees));
        }
    }

    /* Add the remaining trees to the list. Only one list will still have
     * elements. */
    while (!rt_list_is_empty(&heap->trees))
    {
        rt_list_push_back(&all_trees, rt_list_pop_front(&heap->trees));
    }
    while (!rt_list_is_empty(other_trees))
    {
        rt_list_push_back(&all_trees, rt_list_pop_front(other_trees));
    }
    heap->num_trees = 0;

    /* The merge process is similar to binary addition. We iterate across the
     * trees, at each point looking at all the trees of a certain order. If
     * there's exactly one tree of each size, then we just put that in the
     * output list. If there are two, we put nothing, then store the merge of
     * the tree as a "carry." Finally, if there are three, we store one, then
     * use the merge of the other two as a carry.
     *
     * The difference from regular binary addition that we have to consider is
     * that in a skew binomial heap there might be multiple trees of the same
     * order. In the worst case, we'll have four different trees of the same
     * order at any one time (since there's at most two trees of the same order
     * in each heap). Consequently, we'll generalize the logic a bit. For each
     * two trees of a given order that we find, we'll merge them together into
     * a tree of the next highest-order. Any odd tree remaining is left in the
     * output sequence. */
    while (!rt_list_is_empty(&all_trees))
    {
        /* Populate a buffer with trees that have the same order as the first
         * tree left in the worklist. Initially this just has the first tree. */
        RT_LIST(of_same_order);
        rt_list_push_back(&of_same_order, rt_list_pop_front(&all_trees));
        size_t num_same_order = 1;
        size_t order = tree(rt_list_front(&of_same_order))->order;

        /* Move all other trees into this list with the same order. */
        while (!rt_list_is_empty(&all_trees) &&
               (tree(rt_list_front(&all_trees))->order == order))
        {
            rt_list_push_back(&of_same_order, rt_list_pop_front(&all_trees));
            ++num_same_order;
        }

        /* If we have an odd number of trees, output one of them. */
        if ((num_same_order % 2) == 1)
        {
            rt_list_push_back(&heap->trees, rt_list_pop_front(&of_same_order));
            ++heap->num_trees;
        }

        /* Keep merging pairs of remaining trees and putting them back in the
         * worklist. */
        while (!rt_list_is_empty(&of_same_order))
        {
            struct rt_sbheap_node *merged =
                merge_trees(heap, tree(rt_list_pop_front(&of_same_order)),
                            tree(rt_list_pop_front(&of_same_order)));
            rt_list_push_front(&all_trees, &merged->list);
        }
    }
}

struct rt_sbheap_node *rt_sbheap_pop_min(struct rt_sbheap *heap)
{
    if (rt_sbheap_is_empty(heap))
    {
        return NULL;
    }

    /* Search all trees at the top level for the one with the smallest root. */
    struct rt_list *node = rt_list_front(&heap->trees);
    struct rt_sbheap_node *min = tree(node);
    for (node = node->next; node != &heap->trees; node = node->next)
    {
        if (heap->less_than(tree(node), min))
        {
            min = tree(node);
        }
    }

    /* Remove the tree with the smallest root and re-add its children and
     * singletons to the heap. */
    rt_list_remove(&min->list);
    merge_heaps(heap, &min->children);
    while (!rt_list_is_empty(&min->singletons))
    {
        insert_singleton(heap, tree(rt_list_pop_front(&min->singletons)));
    }

    --heap->num_nodes;
    return min;
}

size_t rt_sbheap_size(const struct rt_sbheap *heap)
{
    return heap->num_nodes;
}

bool rt_sbheap_is_empty(const struct rt_sbheap *heap)
{
    return heap->num_nodes == 0;
}

void rt_sbheap_merge(struct rt_sbheap *heap, struct rt_sbheap *other)
{
    merge_heaps(heap, &other->trees);
    heap->num_nodes += other->num_nodes;
    other->num_nodes = 0;
}
