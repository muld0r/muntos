#include <rt/context.h>
#include <rt/cycle.h>
#include <rt/log.h>
#include <rt/rt.h>
#include <rt/syscall.h>

#include <rt/task.h>

#include "coprocessor.h"
#include "vic.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// A flag indicating whether the active task has an fp context.
#ifdef __ARM_FP
volatile bool rt_task_fp_enabled = false;
#endif

struct context
{
#ifdef __ARM_FP
    uint32_t fp_enabled;
#endif
    // Non-volatile regs are pushed last, only if a context switch occurs.
    uint32_t r4, r5, r6, r7, r8, r9, r10, r11;

    // Volatile registers are pushed next, before re-enabling interrupts.
    uint32_t r0, r1, r2, r3, r12;
    void (*lr)(void);

    // pc and cpsr are pushed first by srsdb.
    union
    {
        void (*fn_with_arg)(uintptr_t);
        void (*fn)(void);
    } pc;
    uint32_t cpsr;
};

#ifdef __ARM_BIG_ENDIAN
#define BIG_ENDIAN UINT32_C(1)
#else
#define BIG_ENDIAN UINT32_C(0)
#endif

#define CPSR_MODE_SYS UINT32_C(31)
#define CPSR_E (BIG_ENDIAN << 9)
#define CPSR_THUMB_SHIFT 5

static struct context *context_create(void *stack, size_t stack_size,
                                      uintptr_t fn_addr)
{
    void *const stack_end = (char *)stack + stack_size;
    struct context *ctx = stack_end;
    ctx -= 1;
    ctx->lr = rt_task_exit;

    const uint32_t cpsr_thumb = (fn_addr & 0x1) << CPSR_THUMB_SHIFT;
    ctx->cpsr = CPSR_MODE_SYS | CPSR_E | cpsr_thumb;

#ifdef __ARM_FP
    ctx->fp_enabled = 0;
#endif

    return ctx;
}

void *rt_context_create(void (*fn)(void), void *stack, size_t stack_size)
{
    struct context *const ctx =
        context_create(stack, stack_size, (uintptr_t)fn);
    ctx->pc.fn = fn;
    return ctx;
}

void *rt_context_create_arg(void (*fn)(uintptr_t), uintptr_t arg, void *stack,
                            size_t stack_size)
{
    struct context *const ctx =
        context_create(stack, stack_size, (uintptr_t)fn);
    ctx->pc.fn_with_arg = fn;
    ctx->r0 = arg;
    return ctx;
}

#define STACK_ALIGN 8UL
#define STACK_SIZE(x) (((x) + (STACK_ALIGN - 1)) & ~(STACK_ALIGN - 1))

void rt_start(void)
{
#if RT_TASK_ENABLE_CYCLES
    // Enable counters and reset the cycle counter.
    pmcr_oreq(PMCR_E | PMCR_C);
    // Enable the cycle counter.
    pmcntenset_oreq(PMCNTEN_C);
    rt_task_self()->start_cycle = rt_cycle();
#endif

    // The idle stack needs to be large enough to store a context.
    static char idle_stack[STACK_SIZE(sizeof(struct context))]
        __attribute__((aligned(STACK_ALIGN)));

    // Switch to system mode.
    __asm__("cps %0" : : "i"(CPSR_MODE_SYS));
    // Set the stack pointer to the top of the idle stack.
    __asm__("ldr sp, =%0" : : "i"(&idle_stack[sizeof idle_stack]));

    // Flush memory before enabling interrupts.
    __asm__("dsb" ::: "memory");

    // Enable interrupts.
    __asm__("cpsie i");

    // Invoke a syscall to switch into the first task.
    rt_syscall_pend();

    // Idle loop that will run when no other tasks are runnable.
    for (;;)
    {
        __asm__("wfi");
    }
}

#ifdef __ARM_FP
void rt_task_enable_fp(void)
{
    rt_task_fp_enabled = true;
    /* rt_task_fp_enabled must be set before fpscr. If fpscr is set first and
     * then a context switch occurs, the write to fpscr will be lost. */
    __asm__("dsb" ::: "memory");
    __asm__("vmsr fpscr, %0" : : "r"(0));
}
#endif

void rt_stop(void)
{
    for (;;)
    {
        __asm__("bkpt");
    }
}

void rt_logf(const char *fmt, ...)
{
    (void)fmt;
}

#if RT_ARCH_ARM_R_VIC_TYPE == VIM_SSI

#define SYS_SSIR1 (*(volatile uint32_t *)0xFFFFFFB0U)
#define SYS_SSIR1_KEY (UINT32_C(0x75) << 8)

void rt_syscall_pend(void)
{
    SYS_SSIR1 = SYS_SSIR1_KEY;
    __asm__("dsb");
    __asm__("isb");
}
#endif

uint32_t rt_cycle(void)
{
    return pmccntr();
}
