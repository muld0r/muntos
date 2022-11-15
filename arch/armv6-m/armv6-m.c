#include <rt/context.h>

#include <rt/log.h>
#include <rt/rt.h>
#include <rt/task.h>

#include <stdbool.h>
#include <stdint.h>

struct gp_context
{
    // Saved by task context switch.
    uint32_t r4, r5, r6, r7, r8, r9, r10, r11;

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
    ctx->r0 = (uint32_t)arg;
    ctx->lr = rt_task_exit;
    ctx->pc = fn;
    ctx->psr = 0x01000000U; // thumb state
    /* The context popping process starts from r8. */
    return &ctx->r8;
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
    __attribute__((aligned(8))) static unsigned char idle_stack[64];

    /*
     * Set the process stack pointer to the top of the idle stack and
     * switch to it.
     */
    __asm__("msr psp, %0\n"
            "msr control, %1\n"
            "isb\n"
            :
            : "r"(&idle_stack[sizeof idle_stack]), "r"(2));

    /*
     * Set svcall and pendsv to the lowest exception priority, and systick to
     * one higher. Cortex-M0 has only 2 bits of priority for each exception.
     */
    static volatile uint32_t *const shpr = (volatile uint32_t *)0xE000ED18U;
    shpr[1] = 0xC0000000U;
    shpr[2] = 0x80C00000U;

    /*
     * Enable the systick interrupt.
     * TODO: determine the appropriate reload value after configuring the clock.
     */
    static volatile struct stk *const stk = (volatile struct stk *)0xE000E010U;
    stk->reload = 1000;
    stk->current = 0;
    stk->ctrl = STK_CTRL_ENABLE | STK_CTRL_TICKINT;

    /* Execute a supervisor call to switch into the highest-priority task. */
    __asm__("svc 0");

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
    static volatile uint32_t *const icsr = (volatile uint32_t *)0xE000ED04UL;
    *icsr = PENDSVSET;
    /* Barriers ensure that the PendSV exception is taken before any other code
     * executes, which is required when tasks post a system call. */
    __asm__("dsb\n"
            "isb\n");
}

void rt_logf(const char *fmt, ...)
{
    (void)fmt;
}
