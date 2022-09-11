#include <rt/mutex.h>
#include <rt/sem.h>
#include <rt/sleep.h>
#include <rt/task.h>
#include <rt/rt.h>

static void make_water(void);

struct reaction
{
    struct rt_sem hready, hdone;
    struct rt_mutex m;
};

static struct reaction rxn;

static void reaction_init(void)
{
    rt_sem_init(&rxn.hready, 0);
    rt_sem_init(&rxn.hdone, 0);
    rt_mutex_init(&rxn.m);
}

static void hydrogen(void)
{
    rt_sem_post(&rxn.hready);
    rt_sem_wait(&rxn.hdone);
}

static void oxygen(void)
{
    rt_mutex_lock(&rxn.m);
    rt_sem_wait(&rxn.hready);
    rt_sem_wait(&rxn.hready);
    rt_mutex_unlock(&rxn.m);
    make_water();
    rt_sem_post(&rxn.hdone);
    rt_sem_post(&rxn.hdone);
}

#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static atomic_uint h_bonded = 0;
static atomic_uint o_bonded = 0;
static atomic_uint w_made = 0;

static void make_water(void)
{
    atomic_fetch_add(&w_made, 1);
}

static void hydrogen_fn(void)
{
    hydrogen();
    atomic_fetch_add(&h_bonded, 1);
}

static void oxygen_fn(void)
{
    oxygen();
    atomic_fetch_add(&o_bonded, 1);
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

static void timeout_fn(void)
{
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

    reaction_init();

    struct rt_task atoms[TOTAL_ATOMS];
    static unsigned char stacks[TOTAL_ATOMS][PTHREAD_STACK_MIN];

    for (int i = 0; i < TOTAL_ATOMS; i++)
    {
        if ((unsigned)(rand() % 100) < hydrogen_pct)
        {
            ++hydrogen_atoms;
            rt_task_init(&atoms[i], hydrogen_fn, stacks[i], PTHREAD_STACK_MIN,
                          "hydrogen", 1);
        }
        else
        {
            ++oxygen_atoms;
            rt_task_init(&atoms[i], oxygen_fn, stacks[i], PTHREAD_STACK_MIN,
                          "oxygen", 1);
        }

        rt_task_start(&atoms[i]);
    }

    static unsigned char timeout_stack[PTHREAD_STACK_MIN];
    static RT_TASK(timeout_task, timeout_fn, timeout_stack, 1);
    rt_task_start(&timeout_task);

    unsigned expected_molecules = hydrogen_atoms / 2;
    if (expected_molecules > oxygen_atoms)
    {
        expected_molecules = oxygen_atoms;
    }

    printf("A reaction with %d hydrogen and %d oxygen atoms should produce %d "
           "water molecules.\nRunning reaction...\n",
           hydrogen_atoms, oxygen_atoms, expected_molecules);

    rt_start();

    check(&w_made, "water molecules were produced", expected_molecules);
    check(&h_bonded, "hydrogen atoms were bonded", expected_molecules * 2);
    check(&o_bonded, "oxygen atoms were bonded", expected_molecules);

    puts("Success!\n");
}
