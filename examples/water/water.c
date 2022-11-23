#include "water.h"

#include <rt/atomic.h>
#include <rt/rt.h>
#include <rt/sleep.h>
#include <rt/task.h>

#include <stdbool.h>

static volatile rt_atomic_uint hydrogen_bonded = 0;
static volatile rt_atomic_uint oxygen_bonded = 0;
static volatile rt_atomic_uint water_formed = 0;

void make_water(void)
{
    rt_atomic_fetch_add(&water_formed, 1);
}

static void hydrogen_fn(void *arg)
{
    (void)arg;
    hydrogen();
    rt_atomic_fetch_add(&hydrogen_bonded, 1);
}

static void oxygen_fn(void *arg)
{
    (void)arg;
    oxygen();
    rt_atomic_fetch_add(&oxygen_bonded, 1);
}

static bool check(volatile rt_atomic_uint *p, unsigned expected)
{
    return rt_atomic_load(p) == expected;
}

static void timeout(void *arg)
{
    (void)arg;
    rt_sleep(1000);
    rt_stop();
}

#define TOTAL_ATOMS 200

static struct rt_task atom_tasks[TOTAL_ATOMS];
static char atom_stacks[TOTAL_ATOMS][TASK_STACK_SIZE]
    __attribute__((aligned(STACK_ALIGN)));

int main(void)
{
    unsigned hydrogen_atoms = 0;
    unsigned oxygen_atoms = 0;

    bool hydrogen = true;
    for (unsigned i = 0; i < TOTAL_ATOMS; i++)
    {
        if (hydrogen)
        {
            ++hydrogen_atoms;
            rt_task_init(&atom_tasks[i], hydrogen_fn, NULL, "hydrogen", i + 1,
                         atom_stacks[i], TASK_STACK_SIZE);
        }
        else
        {
            ++oxygen_atoms;
            rt_task_init(&atom_tasks[i], oxygen_fn, NULL, "oxygen", i + 1,
                         atom_stacks[i], TASK_STACK_SIZE);
        }
        hydrogen = !hydrogen;
    }

    __attribute__((aligned(STACK_ALIGN))) static char
        timeout_stack[TASK_STACK_SIZE];
    RT_TASK(timeout, NULL, timeout_stack, 0);

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
