#include "exc_return.h"

#include <rt/context.h>
#include <rt/log.h>
#include <rt/rt.h>
#include <rt/syscall.h>
#include <rt/task.h>

#include <stdbool.h>
#include <stdint.h>

struct gp_context
{
    // Saved by task context switch.
#if (__ARM_ARCH == 8)
    void *psplim;
#endif
    uint32_t r4, r5, r6, r7, r8, r9, r10, r11;

#if defined(__ARM_FP)
    /* Only use a per-task exception return value if floating-point is enabled,
     * because otherwise the exception return value is always the same. */
    uint32_t exc_return;
#endif

    // Saved automatically on exception entry.
    uint32_t r0, r1, r2, r3, r12;
    void (*lr)(void);
    union
    {
        void (*fn_with_arg)(uintptr_t);
        void (*fn)(void);
    } pc;
    uint32_t psr;
};

static void *context_create(void *stack, size_t stack_size)
{
    void *const stack_end = (char *)stack + stack_size;
    struct gp_context *ctx = stack_end;
    ctx -= 1;
#if (__ARM_ARCH == 8)
    ctx->psplim = stack;
#endif
#if defined(__ARM_FP)
    ctx->exc_return = (uint32_t)TASK_INITIAL_EXC_RETURN;
#endif
    ctx->lr = rt_task_exit;
    ctx->psr = 0x01000000U; // thumb state

    return ctx;
}

static void *ctx_begin(struct gp_context *ctx)
{
#if (__ARM_ARCH == 6) || defined(__ARM_ARCH_8M_BASE__)
    /* On armv6-m and armv8-m base, the context popping process starts from r8
     * due to the lack of stmdb. */
    return &ctx->r8;
#else
    return ctx;
#endif
}

void *rt_context_create(void (*fn)(void), void *stack, size_t stack_size)
{
    struct gp_context *ctx = context_create(stack, stack_size);
    ctx->pc.fn = fn;
    return ctx_begin(ctx);
}

void *rt_context_create_arg(void (*fn)(uintptr_t), uintptr_t arg, void *stack,
                            size_t stack_size)
{
    struct gp_context *ctx = context_create(stack, stack_size);
    ctx->pc.fn_with_arg = fn;
    ctx->r0 = arg;
    return ctx_begin(ctx);
}

#define STK_CTRL (*(volatile uint32_t *)0xE000E010U)
#define STK_VAL (*(volatile uint32_t *)0xE000E018U)
#define STK_CTRL_ENABLE 0x1U
#define STK_CTRL_TICKINT 0x2U

#define SHPR ((volatile uint32_t *)0xE000ED18U)
#define SHPR2 (SHPR[1])
#define SHPR3 (SHPR[2])

#define STACK_ALIGN 8UL
#define STACK_SIZE(x) (((x) + (STACK_ALIGN - 1)) & ~(STACK_ALIGN - 1))

void rt_start(void)
{
    // The idle stack needs to be large enough to store a register context.
    static char idle_stack[STACK_SIZE(sizeof(struct gp_context))]
        __attribute__((aligned(STACK_ALIGN)));

    // Set the process stack pointer to the top of the idle stack.
    __asm__("msr psp, %0" : : "r"(&idle_stack[sizeof idle_stack]));
#if (__ARM_ARCH == 8)
    // If supported, set the process stack pointer limit.
    __asm__("msr psplim, %0" : : "r"(idle_stack));
#endif

    // Switch to the process stack pointer.
    __asm__("msr control, %0" : : "r"(2));
    __asm__("isb");

    /*
     * Set SVCall and PendSV to the lowest exception priority, and SysTick to
     * one higher. Write the priority as 0xFF, then read it back to determine
     * how many bits of priority are implemented, and subtract one from that
     * value to get the tick priority.
     */
    SHPR2 = UINT32_C(0xFF) << 24;
    const uint32_t syscall_prio = SHPR2 >> 24;
    const uint32_t tick_prio = syscall_prio - 1;
    SHPR3 = (tick_prio << 24) | (syscall_prio << 16);

    // Reset SysTick and enable its interrupt.
    STK_VAL = 0;
    STK_CTRL = STK_CTRL_ENABLE | STK_CTRL_TICKINT;

    // Enable interrupts.
    __asm__("cpsie i");

    // Execute a supervisor call to switch into the first task.
    __asm__("svc 0");

    // Idle loop that will run when no other tasks are runnable.
    for (;;)
    {
        __asm__("wfi");
    }
}

void rt_stop(void)
{
    for (;;)
    {
        __asm__("bkpt");
    }
}

#define ICSR (*(volatile uint32_t *)0xE000ED04UL)
#define PENDSVSET (1U << 28)

#define IPSR                                                                   \
    ({                                                                         \
        uint32_t ipsr;                                                         \
        __asm__("mrs %0, ipsr" : "=r"(ipsr));                                  \
        ipsr;                                                                  \
    })

void rt_syscall_post(void)
{
    /*
     * If no exception is active, we are in thread mode and should use svc.
     * Otherwise, we are in an exception. Because all exceptions have higher
     * priority than SVCall, using svc will escalate to a hard fault, so we
     * must use PendSV instead.
     */
    if (IPSR == 0)
    {
        __asm__("svc 0");
    }
    else
    {
        ICSR = PENDSVSET;
        /* NOTE: Normally a synchronization barrier is needed after setting
         * ICSR, but an exception return implicitly synchronizes, and this
         * path is only run from a higher priority exception than PendSV. */
    }
}

void rt_logf(const char *fmt, ...)
{
    (void)fmt;
}

#if (__ARM_ARCH == 6)
#include "atomic-v6.c"
#endif
