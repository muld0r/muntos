#pragma once

#include <context_port.h>

void context_init(context_t *ctx, void *stack, size_t stack_size,
                  void (*fn)(void *), void *arg);

// TODO: what do we actually need here?
void context_restore(const context_t *ctx);

void context_save(context_t *ctx);
