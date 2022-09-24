#ifndef RT_H
#define RT_H

#include <rt/task.h>

/*
 * Start the rt scheduler.
 */
void rt_start(void);

/*
 * Stop the rt scheduler.
 */
void rt_stop(void);

/*
 * Yield control of the processor to another runnable task.
 */
void rt_yield(void);

/*
 * Schedule a new task to run. May be called outside of a task.
 */
void rt_sched(void);

#endif /* RT_H */
