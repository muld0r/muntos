#pragma once

#include <stddef.h>

struct rt_context;

/*
 * Return a new context that will execute fn(arg) on the given stack.
 */
struct rt_context *rt_context_create(void *stack, size_t stack_size,
                                       void (*fn)(void));

/*
 * Save the currently-executing context into ctx.
 */
void rt_context_save(struct rt_context *ctx);

/*
 * Load ctx as the current context.
 */
void rt_context_load(struct rt_context *ctx);

/*
 * Destroy the given context. (May be a no-op.)
 */
void rt_context_destroy(struct rt_context *ctx);
