#include <rt/critical.h>

#include <rt/interrupt.h>

static unsigned int critical_nesting = 0;

void rt_critical_begin(void)
{
    rt_interrupt_disable();
    ++critical_nesting;
}

void rt_critical_end(void)
{
    --critical_nesting;
    if (critical_nesting == 0)
    {
        rt_interrupt_enable();
    }
}
