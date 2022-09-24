#ifndef RT_CONTEXT_H
#define RT_CONTEXT_H

#include <stddef.h>

/*
 * Return a new context that will execute fn(arg) on the given stack.
 */
void *rt_context_create(void (*fn)(void *), void *arg, void *stack,
                         size_t stack_size);

#endif /* RT_CONTEXT_H */
