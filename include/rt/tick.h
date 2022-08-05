#pragma once

/*
 * Advance to the next tick. Should be called periodically.
 */
void rt_tick_advance(void);

/*
 * Return the current tick.
 */
unsigned long rt_tick(void);
