#pragma once

#include <limits.h>

typedef unsigned long rt_tick_t;
#define RT_TICK_MAX ((rt_tick_t)ULONG_MAX)

/*
 * Run a tick. Should be called periodically.
 */
void rt_tick(void);

/*
 * Return the current tick number.
 */
rt_tick_t rt_tick_count(void);
