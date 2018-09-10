#pragma once

#include <rt_port.h>

#include <stddef.h>

/*
 * Create a new context the executes fn(arg) with the given stack.
 */
void rt_context_init(rt_context_t *ctx, void *stack, size_t stack_size,
                      void (*fn)(void *), void *arg);

/*
 * Save the caller's context into old_ctx, and begin executing new_ctx.
 * Swapping in old_ctx again will result in the original call to this function
 * returning, running in the context that was originally saved into old_ctx.
 */
void rt_context_swap(rt_context_t *old_ctx, const rt_context_t *new_ctx);
