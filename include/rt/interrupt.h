#ifndef RT_INTERRUPT_H
#define RT_INTERRUPT_H

#include <stdbool.h>

/*
 * Returns true if called from within an interrupt.
 */
bool rt_interrupt_is_active(void);

#endif /* RT_INTERRUPT_H */
