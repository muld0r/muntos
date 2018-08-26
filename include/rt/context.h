#pragma once

#include <rt_port.h>

#include <stddef.h>

void rt_context_init(rt_context_t *ctx, void *stack, size_t stack_size,
                      void (*fn)(void *), void *arg);

void rt_context_swap(rt_context_t *old_ctx, const rt_context_t *new_ctx);
