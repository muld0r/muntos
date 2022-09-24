#include "water.h"

#include <rt/sleep.h>
#include <rt/rt.h>

#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static atomic_uint hydrogen_bonded = 0;
static atomic_uint oxygen_bonded = 0;
static atomic_uint water_formed = 0;

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

static void check(atomic_uint *p, const char *s, unsigned expected)
{
    printf("Checking that %d %s...\n", expected, s);
    unsigned v = atomic_load_explicit(p, memory_order_acquire);
    if (v > expected)
    {
        printf("Too many %s. Expected %d, got %d.\n", s, expected, v);
        exit(1);
    }
    else if (v < expected)
    {
        printf("Not enough %s. Expected %d, got %d.\n", s, expected, v);
        exit(1);
    }
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
    srand((unsigned)getpid() ^ (unsigned)time(NULL));

    unsigned hydrogen_atoms = 0;
    unsigned oxygen_atoms = 0;
    unsigned hydrogen_pct = (unsigned)(round(100 * (double)rand() / RAND_MAX));

    struct reaction *rxn = malloc(4096);
    reaction_init(rxn);

    struct rt_task atoms[TOTAL_ATOMS];
    static char stacks[TOTAL_ATOMS][PTHREAD_STACK_MIN];

    for (int i = 0; i < TOTAL_ATOMS; i++)
    {
        if ((unsigned)(rand() % 100) < hydrogen_pct)
        {
            ++hydrogen_atoms;
            rt_task_init(&atoms[i], hydrogen_fn, rxn, stacks[i],
                          PTHREAD_STACK_MIN, "hydrogen", 1);
        }
        else
        {
            ++oxygen_atoms;
            rt_task_init(&atoms[i], oxygen_fn, rxn, stacks[i],
                          PTHREAD_STACK_MIN, "oxygen", 1);
        }
    }

    static char timeout_stack[PTHREAD_STACK_MIN];
    RT_TASK(timeout, NULL, timeout_stack, 1);

    unsigned expected_water = hydrogen_atoms / 2;
    if (expected_water > oxygen_atoms)
    {
        expected_water = oxygen_atoms;
    }

    printf("A reaction with %d hydrogen and %d oxygen atoms should form %d "
           "water molecules.\nRunning reaction...\n",
           hydrogen_atoms, oxygen_atoms, expected_water);

    rt_start();

    check(&water_formed, "water molecules were formed", expected_water);
    check(&hydrogen_bonded, "hydrogen atoms were bonded", expected_water * 2);
    check(&oxygen_bonded, "oxygen atoms were bonded", expected_water);

    puts("Success!\n");
}
