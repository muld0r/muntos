#pragma once

#include <rt/rt.h>

/*
 * Sleep the current task for a given number of ticks.
 */
void rt_sleep(unsigned long ticks);

/*
 * Sleep the current task until *last_wake_tick + period.
 * *last_wake_tick will be set to the next wakeup tick.
 */
void rt_sleep_periodic(unsigned long *last_wake_tick, unsigned long period);
