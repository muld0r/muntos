#ifndef RT_ARCH_STACK_H
#define RT_ARCH_STACK_H

#include <limits.h>

/* x86_64 and aarch64 both have a 16-byte alignment requirement */
#ifndef RT_STACK_ALIGN
#define RT_STACK_ALIGN(n) 16UL
#endif

#define RT_STACK_MIN PTHREAD_STACK_MIN

#ifdef __MACH__
#define RT_STACK_SECTION(name) "0,.bss"
#else
#define RT_STACK_SECTION(name) ".bss." #name
#endif

#endif /* RT_ARCH_STACK_H */
