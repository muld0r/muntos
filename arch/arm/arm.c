#include <rt/context.h>

#include <rt/rt.h>

#include <stdbool.h>

struct gp_context
{
    // Saved by task context switch.
    uint32_t r4, r5, r6, r7, r8, r9, r10, r11, exc_lr;

    // Saved automatically on exception entry.
    uint32_t r0, r1, r2, r3, r12;
    void (*lr)(void);
    void (*pc)(void);
    uint32_t xpsr;
};

void *rt_context_create(void (*fn)(void), void *stack, size_t stack_size)
{
    void *const stack_end = (char *)stack + stack_size;
    struct gp_context *ctx = stack_end;
    ctx -= 1;
    ctx->xpsr = 0x01000000U;
    ctx->pc = fn;
    ctx->lr = rt_task_exit;
    ctx->exc_lr = 0xFFFFFFFDU; // thread mode, no FP, use PSP
    return ctx;
}

void idle_fn(void)
{
    for (;;)
    {
        __asm__("wfi");
    }
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

static volatile struct stk *stk = (volatile struct stk *)0xE000E010U;

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

static volatile struct shpr *shpr = (volatile struct shpr *)0xE000ED18U;

void rt_start(void)
{
    // TODO: make this smaller
    __attribute__((aligned(8))) static char idle_stack[512];

    __asm__ __volatile__(
        // Set the process stack pointer to the top of the idle task stack.
        "msr psp, %0\n"
        // Switch to the process stack pointer.
        "mov r0, 2\n"
        "msr control, r0\n"
        "isb\n"
        :
        : "r"(&idle_stack[sizeof idle_stack])
        : "r0");

    shpr->svcall = 1 << 4;
    shpr->systick = 2 << 4;
    __asm__("isb");

    stk->reload = 1000000;
    stk->current = 0;
    stk->ctrl = STK_CTRL_ENABLE | STK_CTRL_TICKINT;

    // Drop privileges (and keep using the process stack pointer).
    __asm__("mov r0, 3\n"
            "msr control, r0\n"
            "isb\n"
            :
            :
            : "r0");

    rt_yield();
    idle_fn();
}

void rt_stop(void)
{
    // TODO: Disable the tick and syscall interrupts, return execution
    // to the end of rt_start.
}

void rt_syscall_post(void)
{
    __asm__("svc 0");
}
