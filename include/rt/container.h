#pragma once

#include <stddef.h>
#include <stdint.h>

#define container_item(ptr, type, member)                                      \
  ((type *)((uintptr_t)(ptr) - (uintptr_t)(&((type *)NULL)->member)))
