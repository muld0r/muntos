#pragma once

#include <stddef.h>

/*
 * Store a new context into *ctx that will execute fn(arg) on the given stack.
 */
void rt_context_init(void **ctx, void *stack, size_t stack_size,
                      void (*fn)(void *), void *arg);

/*
 * Save the caller's context into *old_ctx, and begin executing new_ctx.
 * Swapping in old_ctx again will result in the original call to this function
 * returning, running the context that was originally saved into *old_ctx.
 */
void rt_context_swap(void **old_ctx, void *new_ctx);
