#ifndef RT_CONTEXT_H
#define RT_CONTEXT_H

#include <stddef.h>

/*
 * Return a new context that will execute fn() on the given stack.
 */
void *rt_context_create(void (*fn)(void), void *stack, size_t stack_size);

/*
 * Return a new context that will execute fn(arg) on the given stack.
 */
void *rt_context_create_arg(void (*fn)(void *), void *stack, size_t stack_size, void *arg);

#endif /* RT_CONTEXT_H */
