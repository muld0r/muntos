#include <rt/context.h>

#include <rt/rt.h>

#include <stdbool.h>
#include <stdint.h>

struct gp_context
{
    // Saved by task context switch.
    uintptr_t r4, r5, r6, r7, r8, r9, r10, r11, exc_lr;

    // Saved automatically on exception entry.
    uintptr_t r0, r1, r2, r3, r12;
    void (*lr)(void);
    void (*pc)(void *);
    uintptr_t psr;
};

void *rt_context_create(void (*fn)(void *), void *arg, void *stack,
                        size_t stack_size)
{
    void *const stack_end = (char *)stack + stack_size;
    struct gp_context *ctx = stack_end;
    ctx -= 1;
    ctx->exc_lr = 0xFFFFFFFDU; // thread mode, no FP, use PSP
    ctx->r0 = (uintptr_t)arg;
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

static volatile struct stk *const stk = (volatile struct stk *)0xE000E010U;

struct shpr
{
    uint8_t mem_manage;
    uint8_t bus_fault;
    uint8_t usage_fault;
    uint8_t : 8;
    uint8_t : 8;
    uint8_t : 8;
    uint8_t : 8;
    uint8_t svcall;
    uint8_t : 8;
    uint8_t : 8;
    uint8_t pendsv;
    uint8_t systick;
};

static volatile struct shpr *const shpr = (volatile struct shpr *)0xE000ED18U;

__attribute__((noreturn)) void rt_start(void)
{
    /* The idle stack needs to be large enough to store a register context. */
    __attribute__((aligned(8))) static unsigned char idle_stack[72];

    /*
     * Set the process stack pointer to the top of the idle stack and
     * switch to it.
     */
    __asm__("msr psp, %0\n"
            "mov r0, 2\n"
            "msr control, r0\n"
            "isb\n"
            :
            : "r"(&idle_stack[sizeof idle_stack])
            : "r0");

    /*
     * Set svcall and pendsv to the lowest exception priority, and systick to
     * one higher.
     */
    shpr->svcall = 15 << 4;
    shpr->pendsv = 15 << 4;
    shpr->systick = 14 << 4;

    /*
     * Enable the systick interrupt.
     * TODO: determine the appropriate reload value after configuring the clock.
     */
    stk->reload = 1000;
    stk->current = 0;
    stk->ctrl = STK_CTRL_ENABLE | STK_CTRL_TICKINT;

    /*
     * Drop privileges (and keep using the process stack pointer).
     * Then execute a supervisor call to switch into the first task.
     * TODO: is the isb required when dropping privileges prior to an svc?
     */
    __asm__("mov r0, 3\n"
            "msr control, r0\n"
            "isb\n"
            "svc 0\n"
            :
            :
            : "r0");

    /* Idle loop that will run when no other tasks are runnable. */
    for (;;)
    {
        __asm__("wfi");
    }
}

void rt_stop(void)
{
    __asm__("bkpt");
}

static volatile uint32_t *const icsr = (volatile uint32_t *)0xE000ED04UL;
#define PENDSVSET (1U << 28)

void rt_syscall_post(void)
{
    /*
     * If no exception is active, we are in unprivileged mode and must use svc.
     * Otherwise, we are in an exception. Because all exceptions have higher
     * priority than SVCall, using svc will escalate to a hard fault, so we
     * must use PendSV instead.
     */
    uintptr_t ipsr;
    __asm__("mrs %0, ipsr" : "=r"(ipsr));
    if (ipsr == 0)
    {
        __asm__("svc 0");
    }
    else
    {
        *icsr = PENDSVSET;
        __asm__("dsb\n"
                "isb\n");
    }
}
