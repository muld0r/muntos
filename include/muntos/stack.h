#ifndef RT_STACK_H
#define RT_STACK_H

#include <muntos/arch/stack.h>

#ifndef RT_STACK_SIZE
#define RT_STACK_SIZE(n)                                                       \
    (((n) + (RT_STACK_ALIGN(n) - 1)) & ~(RT_STACK_ALIGN(n) - 1))
#endif

#ifndef RT_STACK_SECTION
#define RT_STACK_SECTION(name) ".stack." #name
#endif

#define RT_STACK(name, size)                                                   \
    static char name[RT_STACK_SIZE(size)]                                      \
        __attribute__((aligned(RT_STACK_ALIGN(size)),                          \
                       section(RT_STACK_SECTION(name))))

#define RT_STACKS(name, size, num)                                             \
    static char name[(num)][RT_STACK_SIZE(size)]                               \
        __attribute__((aligned(RT_STACK_ALIGN(size)),                          \
                       section(RT_STACK_SECTION(name))))

#endif /* RT_STACK_H */
