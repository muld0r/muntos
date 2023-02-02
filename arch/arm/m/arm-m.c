#include <rt/context.h>
#include <rt/cycle.h>
#include <rt/log.h>
#include <rt/rt.h>
#include <rt/syscall.h>

#include <rt/task.h>

#include <stdbool.h>
#include <stdint.h>

#include "exc_return.h"

struct context
{
    // Saved by task context switch.
#if __ARM_ARCH == 8
    void *psplim;
#endif
    uint32_t r4, r5, r6, r7, r8, r9, r10, r11;

#ifdef __ARM_FP
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

#define PSR_THUMB (UINT32_C(1) << 24)

static struct context *context_create(void *stack, size_t stack_size)
{
    void *const stack_end = (char *)stack + stack_size;
    struct context *ctx = stack_end;
    ctx -= 1;

#if __ARM_ARCH == 8
    ctx->psplim = stack;
#endif

#ifdef __ARM_FP
    ctx->exc_return = (uint32_t)TASK_INITIAL_EXC_RETURN;
#endif

    ctx->lr = rt_task_exit;
    ctx->psr = PSR_THUMB;
    return ctx;
}

void *rt_context_create(void (*fn)(void), void *stack, size_t stack_size)
{
    struct context *const ctx = context_create(stack, stack_size);
    ctx->pc.fn = fn;
    return ctx;
}

void *rt_context_create_arg(void (*fn)(uintptr_t), uintptr_t arg, void *stack,
                            size_t stack_size)
{
    struct context *const ctx = context_create(stack, stack_size);
    ctx->pc.fn_with_arg = fn;
    ctx->r0 = arg;
    return ctx;
}

#define STK_CTRL (*(volatile uint32_t *)0xE000E010U)
#define STK_VAL (*(volatile uint32_t *)0xE000E018U)
#define STK_CTRL_ENABLE (UINT32_C(1) << 0)
#define STK_CTRL_TICKINT (UINT32_C(1) << 1)

#define SHPR ((volatile uint32_t *)0xE000ED18U)
#define SHPR2 (SHPR[1])
#define SHPR3 (SHPR[2])

#define STACK_ALIGN 8UL
#define STACK_SIZE(x) (((x) + (STACK_ALIGN - 1)) & ~(STACK_ALIGN - 1))

void rt_start(void)
{
    // The idle stack needs to be large enough to store a context.
    static char idle_stack[STACK_SIZE(sizeof(struct context))]
        __attribute__((aligned(STACK_ALIGN)));

    // Set the process stack pointer to the top of the idle stack.
    __asm__("msr psp, %0" : : "r"(&idle_stack[sizeof idle_stack]));
#if __ARM_ARCH == 8
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
#define PENDSVSET (UINT32_C(1) << 28)

#define IPSR                                                                   \
    ({                                                                         \
        uint32_t ipsr;                                                         \
        __asm__ __volatile__("mrs %0, ipsr" : "=r"(ipsr));                     \
        ipsr;                                                                  \
    })

void rt_syscall_pend(void)
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

#define DWT_LAR (*(volatile uint32_t *)0xE0001FB0)
#define DWT_LAR_UNLOCK UINT32_C(0xC5ACCE55)

#define DEMCR (*(volatile unsigned *)0xE000EDFCU)
#define DEMCR_TRCENA (UINT32_C(1) << 24)

#define DWT_CTRL (*(volatile uint32_t *)0xE0001000U)
#define DWT_CTRL_CYCCNTENA (UINT32_C(1) << 0)

#define DWT_CYCCNT (*(volatile uint32_t *)0xE0001004U)

void rt_cycle_enable(void)
{
    DWT_LAR = DWT_LAR_UNLOCK;
    DEMCR |= DEMCR_TRCENA;
    DWT_CTRL |= DWT_CTRL_CYCCNTENA;
}

uint32_t rt_cycle(void)
{
    return DWT_CYCCNT;
}

#if __ARM_ARCH == 6
#include "atomic-v6.c"
#endif
