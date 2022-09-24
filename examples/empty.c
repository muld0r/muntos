#include <rt/rt.h>

#include <limits.h>

static void empty(void)
{
    rt_stop();
}

int main(void)
{
    static char stack[PTHREAD_STACK_MIN];
    RT_TASK(empty, stack, 1);
    rt_start();
}
