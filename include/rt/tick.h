#pragma once

// Implemented by a port to cause rt_tick() to be called at a regular
// interval.
void rt_tick_start(void);

void rt_tick(void);
