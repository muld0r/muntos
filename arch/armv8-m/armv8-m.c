#include <rt/context.h>

#include <rt/log.h>
#include <rt/rt.h>
#include <rt/task.h>

#include <stdbool.h>
#include <stdint.h>

struct gp_context
{
    // Saved by task context switch.
    uintptr_t psplim;
    uint32_t r4, r5, r6, r7, r8, r9, r10, r11;

#if defined(__ARM_FP)
    /* Only use a per-task exception lr if floating-point is enabled, because
     * otherwise the exception return value is always the same. */
    uint32_t exc_lr;
#endif

    // Saved automatically on exception entry.
    uint32_t r0, r1, r2, r3, r12;
    void (*lr)(void);
    void (*pc)(void *);
    uint32_t psr;
};

void *rt_context_create(void (*fn)(void *), void *arg, void *stack,
                        size_t stack_size)
{
    void *const stack_end = (char *)stack + stack_size;
    struct gp_context *ctx = stack_end;
    ctx -= 1;
    ctx->psplim = (uintptr_t)stack;
#if defined(__ARM_FP)
    ctx->exc_lr = 0xFFFFFFBCU; // non-secure stack, no FP, use PSP, non-secure exception
#endif
    ctx->r0 = (uint32_t)arg;
    ctx->lr = rt_task_exit;
    ctx->pc = fn;
    ctx->psr = 0x01000000U; // thumb state
    return ctx;
}

#define STK_CTRL_ENABLE 0x1U
#define STK_CTRL_TICKINT 0x2U
#define STK_CTRL_CLKSOURCE 0x4U
#define STK_CTRL_COUNTFLAG 0x100U
#define STK_CALIB_SKEW 0x40000000U
#define STK_CALIB_NOREF 0x80000000U

struct stk
{
    uint32_t ctrl;
    uint32_t reload;
    uint32_t current;
    uint32_t calib;
};

__attribute__((noreturn)) void rt_start(void)
{
    /* The idle stack needs to be large enough to store a register context. */
    __attribute__((aligned(8))) static unsigned char idle_stack[72];

    /*
     * Set psp and psplim to the bounds of the idle stack and switch to it.
     */
    __asm__("msr psp, %0\n"
            "msr psplim, %1\n"
            "msr control, %2\n"
            "isb\n"
            :
            : "r"(&idle_stack[sizeof idle_stack]), "r"(idle_stack), "r"(2));

    /*
     * Set svcall and pendsv to the lowest exception priority, and systick to
     * one higher. STM32F4 implements only 4 bits of priority for each
     * exception. TODO: make this configurable?
     */
    static volatile uint32_t *const shpr = (uint32_t *)0xE000ED18U;
    shpr[1] = 0xE0000000U;
    shpr[2] = 0xC0E00000U;

    /*
     * Enable the systick interrupt.
     * TODO: determine the appropriate reload value after configuring the clock.
     */
    static volatile struct stk *const stk = (volatile struct stk *)0xE000E010U;
    stk->reload = 1000;
    stk->current = 0;
    stk->ctrl = STK_CTRL_ENABLE | STK_CTRL_TICKINT;

    /*
     * Drop privileges (and keep using the process stack pointer).
     * Then execute a supervisor call to switch into the first task.
     * TODO: is the isb required when dropping privileges prior to an svc?
     */
    __asm__("msr control, %0\n"
            "isb\n"
            "svc 0\n"
            :
            : "r"(3));

    /* Idle loop that will run when no other tasks are runnable. */
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

#define PENDSVSET (1U << 28)

void rt_syscall_post(void)
{
    /*
     * If no exception is active, we are in unprivileged mode and must use svc.
     * Otherwise, we are in an exception. Because all exceptions have higher
     * priority than SVCall, using svc will escalate to a hard fault, so we
     * must use PendSV instead.
     */
    uint32_t ipsr;
    __asm__("mrs %0, ipsr" : "=r"(ipsr));
    if (ipsr == 0)
    {
        __asm__("svc 0");
    }
    else
    {
        static volatile uint32_t *const icsr =
            (volatile uint32_t *)0xE000ED04UL;
        *icsr = PENDSVSET;
        /* TODO: are these needed? This code path is run from an exception with
         * higher priority than PendSV, and that exception must execute an
         * exception return before the PendSV handler gets to run. */
        __asm__("dsb\n"
                "isb\n");
    }
}

void rt_logf(const char *fmt, ...)
{
    (void)fmt;
}
