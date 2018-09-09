#pragma once

#include <rt/rt.h>

/*
 * Delay the current task for a given number of ticks.
 */
void rt_delay(rt_tick_t delay);

/*
 * Delay the current task until *last_wake_tick + period.
 * *last_wake_tick will be set to the next wakeup tick.
 */
void rt_delay_periodic(rt_tick_t *last_wake_tick, rt_tick_t period);

/*
 * Wake up delayed tasks whose delay timers have expired.
 */
void rt_delay_wake_tasks(void);
