#ifndef RT_CONTAINER_H
#define RT_CONTAINER_H

#include <stddef.h>
#include <stdint.h>

#define rt_container_of(ptr, type, member)                                    \
    ((type *)((uintptr_t)(ptr)-offsetof(type, member)))

#endif /* RT_CONTAINER_H */
