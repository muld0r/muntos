#ifndef RT_CONTEXT_H
#define RT_CONTEXT_H

#include <stddef.h>
#include <stdint.h>

/*
 * Return a new context that will execute fn() on the given stack.
 */
void *rt_context_create(void (*fn)(void), void *stack, size_t stack_size);

/*
 * Return a new context that will execute fn(arg) on the given stack.
 */
void *rt_context_create_arg(void (*fn)(uintptr_t), uintptr_t arg, void *stack,
                            size_t stack_size);

/*
 * Pointer to the previous task's context field, used to store the suspending
 * context during a context switch.
 */
extern void **rt_prev_context;

#endif /* RT_CONTEXT_H */
