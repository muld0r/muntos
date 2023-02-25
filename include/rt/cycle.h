#ifndef RT_CYCLE_H
#define RT_CYCLE_H

#include <stdint.h>

#ifndef RT_CYCLE_ENABLE
#define RT_CYCLE_ENABLE 0
#endif

uint32_t rt_cycle(void);

#endif /* RT_CYCLE_H */
