#include "vic.h"

#include <rt/context.h>
#include <rt/log.h>
#include <rt/rt.h>
#include <rt/syscall.h>
#include <rt/task.h>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#if defined(__ARM_FP)
#define USE_CONTEXT_FLAGS 1
#else
#define USE_CONTEXT_FLAGS 0
#endif

#if USE_CONTEXT_FLAGS
uint32_t rt_context_flags = 0;
#endif

struct context
{
    // Context flags are pushed along with the non-volatile regs.
#if USE_CONTEXT_FLAGS
    uint32_t flags;
#endif
    // Non-volatile regs are pushed last, only if a context switch occurs.
    uint32_t r4, r5, r6, r7, r8, r9, r10, r11;

    // Remaining volatile regs are pushed after re-enabling interrupts.
    uint32_t stack_adjust, r3, r12;
    void (*lr)(void);

    // If a gap is required for stack alignment, it will be here.

    // These registers are saved first, before re-enabling interrupts.
    uint32_t r0, r1, r2;

    // pc and cpsr are pushed first by srsdb.
    union
    {
        void (*fn_with_arg)(uintptr_t);
        void (*fn)(void);
    } pc;
    uint32_t cpsr;
};

#if defined(__ARM_BIG_ENDIAN)
#define BIG_ENDIAN UINT32_C(1)
#else
#define BIG_ENDIAN UINT32_C(0)
#endif

#define CPSR_MODE_SYS UINT32_C(0x1F)
#define CPSR_E (BIG_ENDIAN << 9)
#define CPSR_THUMB_SHIFT 5

static struct context *context_create(void *stack, size_t stack_size,
                                      uintptr_t fn_addr)
{
    void *const stack_end = (char *)stack + stack_size;
    struct context *ctx = stack_end;
    ctx -= 1;
    ctx->stack_adjust = 0;
    ctx->lr = rt_task_exit;

    const uint32_t cpsr_thumb = (fn_addr & 0x1) << CPSR_THUMB_SHIFT;
    ctx->cpsr = CPSR_MODE_SYS | CPSR_E | cpsr_thumb;

#if USE_CONTEXT_FLAGS
    ctx->flags = 0;
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

#if defined(__ARM_FP)
void rt_enable_fpu(void)
{
    rt_context_flags |= 1;
    /* Force the update of rt_context_flags to occur before setting fpscr.
     * If fpscr is set and a context switch occurs before the context flag is
     * updated, the write to fpscr will be lost. */
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
