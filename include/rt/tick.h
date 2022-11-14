#ifndef RT_TICK_H
#define RT_TICK_H

/*
 * Advance to the next tick. Should be called periodically.
 */
void rt_tick_advance(void);

/*
 * Return the current tick.
 */
unsigned long rt_tick(void);

#endif /* RT_TICK_H */
