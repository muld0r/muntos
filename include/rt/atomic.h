#ifndef RT_ATOMIC_H
#define RT_ATOMIC_H

#include <stdint.h>

#include <stdatomic.h>
#include <stdbool.h>

typedef atomic_bool rt_atomic_bool;
typedef atomic_uchar rt_atomic_uchar;
typedef atomic_int rt_atomic_int;
typedef atomic_uint rt_atomic_uint;
typedef atomic_long rt_atomic_long;
typedef atomic_ulong rt_atomic_ulong;
typedef atomic_size_t rt_atomic_size_t;
typedef _Atomic uint32_t rt_atomic_uint32_t;

#define rt_atomic_load atomic_load
#define rt_atomic_store atomic_store
#define rt_atomic_load_explicit atomic_load_explicit
#define rt_atomic_store_explicit atomic_store_explicit

#define rt_atomic_fetch_add atomic_fetch_add
#define rt_atomic_fetch_add_explicit atomic_fetch_add_explicit

#define rt_atomic_fetch_sub atomic_fetch_sub
#define rt_atomic_fetch_sub_explicit atomic_fetch_sub_explicit

#define rt_atomic_fetch_and atomic_fetch_and
#define rt_atomic_fetch_and_explicit atomic_fetch_and_explicit

#define rt_atomic_fetch_or atomic_fetch_or
#define rt_atomic_fetch_or_explicit atomic_fetch_or_explicit

#define rt_atomic_exchange_explicit atomic_exchange_explicit
#define rt_atomic_compare_exchange_weak_explicit                               \
    atomic_compare_exchange_weak_explicit
#define rt_atomic_compare_exchange_strong_explicit                             \
    atomic_compare_exchange_strong_explicit

/*
 * Work around a bug in gcc where atomic_flag operations silently don't
 * generate atomic code on armv6-m rather than failing to link. The equivalent
 * atomic_exchange operations on an atomic_bool work.
 *
 * https://gcc.gnu.org/bugzilla/show_bug.cgi?id=107567
 */
#define rt_atomic_flag atomic_bool
#define RT_ATOMIC_FLAG_INIT false
#define rt_atomic_flag_test_and_set_explicit(f, mo)                            \
    atomic_exchange_explicit(f, true, mo)
#define rt_atomic_flag_clear_explicit(f, mo) atomic_store_explicit(f, false, mo)

#endif /* RT_ATOMIC_H */
