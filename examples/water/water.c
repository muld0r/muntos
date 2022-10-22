#include "water.h"

#include <rt/rt.h>
#include <rt/sleep.h>
#include <rt/task.h>

#include <stdatomic.h>

static volatile atomic_uint hydrogen_bonded = 0;
static volatile atomic_uint oxygen_bonded = 0;
static volatile atomic_uint water_formed = 0;

void make_water(void)
{
    atomic_fetch_add(&water_formed, 1);
}

static void hydrogen_fn(void *arg)
{
    hydrogen(arg);
    atomic_fetch_add(&hydrogen_bonded, 1);
}

static void oxygen_fn(void *arg)
{
    oxygen(arg);
    atomic_fetch_add(&oxygen_bonded, 1);
}

static bool check(volatile atomic_uint *p, unsigned expected)
{
    return atomic_load(p) == expected;
}

static void timeout(void *arg)
{
    (void)arg;
    rt_sleep(1000);
    rt_stop();
}

#define TOTAL_ATOMS 200

int main(void)
{
    unsigned hydrogen_atoms = 0;
    unsigned oxygen_atoms = 0;

    __attribute__((aligned(8))) static char rxnbuf[1024];
    struct reaction *rxn = (void *)rxnbuf;
    reaction_init(rxn);

    static struct rt_task atoms[TOTAL_ATOMS];
    __attribute__((aligned(STACK_ALIGN))) static char stacks[TOTAL_ATOMS]
                                                            [TASK_STACK_SIZE];

    bool hydrogen = true;
    for (int i = 0; i < TOTAL_ATOMS; i++)
    {
        if (hydrogen)
        {
            ++hydrogen_atoms;
            rt_task_init(&atoms[i], hydrogen_fn, rxn, "hydrogen", 1, stacks[i],
                         TASK_STACK_SIZE);
        }
        else
        {
            ++oxygen_atoms;
            rt_task_init(&atoms[i], oxygen_fn, rxn, "oxygen", 1, stacks[i],
                         TASK_STACK_SIZE);
        }
        hydrogen = !hydrogen;
    }

    __attribute__((aligned(STACK_ALIGN))) static char
        timeout_stack[TASK_STACK_SIZE];
    RT_TASK(timeout, NULL, timeout_stack, 1);

    unsigned expected_water = hydrogen_atoms / 2;
    if (expected_water > oxygen_atoms)
    {
        expected_water = oxygen_atoms;
    }

    rt_start();

    if (!check(&water_formed, expected_water) ||
        !check(&hydrogen_bonded, expected_water * 2) ||
        !check(&oxygen_bonded, expected_water))
    {
        return 1;
    }
}
