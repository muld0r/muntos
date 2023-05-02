#include <rt/context.h>
#include <rt/cycle.h>
#include <rt/interrupt.h>
#include <rt/log.h>
#include <rt/mpu.h>
#include <rt/rt.h>
#include <rt/stack.h>
#include <rt/syscall.h>
#include <rt/task.h>

#define PROFILE_R (__ARM_ARCH_PROFILE == 'R')
#define PROFILE_M (__ARM_ARCH_PROFILE == 'M')

#define V8M ((__ARM_ARCH == 8) && PROFILE_M)

#ifdef __ARM_FP
#define FPU 1
#else
#define FPU 0
#endif

#if PROFILE_R
#include "r/coprocessor.h"
#include "r/vic.h"
#elif PROFILE_M
#include "m/exc_return.h"
#endif

#include <stdbool.h>
#include <stdint.h>

struct context
{
#if PROFILE_M && RT_MPU_ENABLE
    /* In M-profile, thread-mode privilege is part of the control register. In
     * A/R-profile, it's part of the processor mode (system vs. user), which is
     * a field of the CPSR. */
    uint32_t control;
#endif

#if V8M
    void *psplim;
#endif

#if PROFILE_R && FPU
    uint32_t fp_enabled;
#endif

    uint32_t r4, r5, r6, r7, r8, r9, r10, r11;

#if PROFILE_M && FPU
    /* Only use a per-task exception return value if floating-point is enabled,
     * because otherwise the exception return value is always the same. This
     * is the lr value on exception entry, so place it after r4-r11 so it can
     * be saved/restored along with those registers. */
    uint32_t exc_return;
#endif

    uint32_t r0, r1, r2, r3, r12;
    void (*lr)(void);
    union
    {
        void (*fn_with_arg)(uintptr_t);
        void (*fn)(void);
    } pc;
    uint32_t psr;
};

#if PROFILE_R

#define CPSR_MODE_USR UINT32_C(16)
#define CPSR_MODE_SYS UINT32_C(31)
#define CPSR_MODE_MASK UINT32_C(0x1F)
#define CPSR_E ((uint32_t)(__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) << 9)
#define CPSR_THUMB(fn_addr) ((uint32_t)((fn_addr)&1) << 5)

#elif PROFILE_M

#define CONTROL_NPRIV (UINT32_C(1) << 0)
#define CONTROL_SPSEL (UINT32_C(1) << 1)

#define PSR_THUMB (UINT32_C(1) << 24)

#define STK_CTRL (*(volatile uint32_t *)0xE000E010U)
#define STK_VAL (*(volatile uint32_t *)0xE000E018U)
#define STK_CTRL_ENABLE (UINT32_C(1) << 0)
#define STK_CTRL_TICKINT (UINT32_C(1) << 1)

#define SHPR ((volatile uint32_t *)0xE000ED18U)
#define SHPR2 (SHPR[1])
#define SHPR3 (SHPR[2])

#define DWT_LAR (*(volatile uint32_t *)0xE0001FB0)
#define DWT_LAR_UNLOCK UINT32_C(0xC5ACCE55)

#define DEMCR (*(volatile unsigned *)0xE000EDFCU)
#define DEMCR_TRCENA (UINT32_C(1) << 24)

#define DWT_CTRL (*(volatile uint32_t *)0xE0001000U)
#define DWT_CTRL_CYCCNTENA (UINT32_C(1) << 0)

#define DWT_CYCCNT (*(volatile uint32_t *)0xE0001004U)

#endif // PROFILE

static struct context *context_create(void *stack, size_t stack_size,
                                      uintptr_t fn_addr)
{
    void *const stack_end = (char *)stack + stack_size;
    struct context *ctx = stack_end;
    ctx -= 1;

#if V8M
    ctx->psplim = stack;
#endif

    ctx->lr = rt_task_exit;

#if PROFILE_R
    ctx->psr = CPSR_MODE_SYS | CPSR_E | CPSR_THUMB(fn_addr);
#if FPU
    ctx->fp_enabled = 0;
#endif

#elif PROFILE_M
    (void)fn_addr;
    ctx->psr = PSR_THUMB;
#if RT_MPU_ENABLE
    // Tasks start privileged and use the process stack pointer.
    ctx->control = CONTROL_SPSEL;
#endif
#if FPU
    ctx->exc_return = (uint32_t)TASK_INITIAL_EXC_RETURN;
#endif

#endif // PROFILE
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

void rt_start(void)
{
#if PROFILE_M
    /*
     * Set PendSV to the lowest exception priority and SysTick to one higher.
     * Write the priority as 0xFF, then read it back to determine how many bits
     * of priority are implemented, and subtract one from that value to get the
     * tick priority.
     */
    SHPR3 = UINT32_C(0xFF) << 16;
    const uint32_t shpr3 = SHPR3;
    const uint32_t syscall_prio = shpr3 >> 16;
    const uint32_t tick_prio = syscall_prio - 1;
    SHPR3 = (tick_prio << 24) | shpr3;

    // Reset SysTick and enable its interrupt.
    STK_VAL = 0;
    STK_CTRL = STK_CTRL_ENABLE | STK_CTRL_TICKINT;
#endif

#if RT_CYCLE_ENABLE
#if PROFILE_R
    // Enable counters and reset the cycle counter.
    pmcr_oreq(PMCR_E | PMCR_C);
    // Enable the cycle counter.
    pmcntenset_oreq(PMCNTEN_C);
#elif PROFILE_M
    // Enable the cycle counter.
    DWT_LAR = DWT_LAR_UNLOCK;
    DEMCR |= DEMCR_TRCENA;
    DWT_CTRL |= DWT_CTRL_CYCCNTENA;
#endif // PROFILE
#endif // RT_CYCLE_ENABLE

#if RT_TASK_ENABLE_CYCLE
    rt_task_self()->start_cycle = rt_cycle();
#endif

    // The idle task stack needs to be large enough to store a context.
    RT_STACK(idle_task_stack, sizeof(struct context));

#if V8M
    // If supported, set the process stack pointer limit.
    __asm__("msr psplim, %0" : : "r"(idle_task_stack));
#endif

#if PROFILE_R
    // Switch to system mode.
    __asm__("cps %0" : : "i"(CPSR_MODE_SYS));
    // Set the stack pointer to the top of the idle stack.
    __asm__("ldr sp, =%0" : : "i"(&idle_task_stack[sizeof idle_task_stack]));
#elif PROFILE_M
    // Set the process stack pointer to the top of the idle stack.
    __asm__("msr psp, %0" : : "r"(&idle_task_stack[sizeof idle_task_stack]));
    // Switch to the process stack pointer.
    __asm__("msr control, %0" : : "r"(CONTROL_SPSEL));
    __asm__("isb");
#endif

    // Flush memory before enabling interrupts.
    __asm__("dsb" ::: "memory");

    // Enable interrupts.
    __asm__("cpsie i");

    rt_task_drop_privilege();

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

#if PROFILE_R
static inline uint32_t cpsr_mode(void)
{
    uint32_t cpsr;
    __asm__ __volatile__("mrs %0, cpsr" : "=r"(cpsr));
    return cpsr & CPSR_MODE_MASK;
}
#endif

bool rt_interrupt_is_active(void)
{
#if PROFILE_R
    const uint32_t mode = cpsr_mode();
    /*
     * NOTE: this assumes that nested interrupts don't use system mode.
     * Interrupt nesting should use supervisor mode, which doesn't require each
     * task stack to accommodate interrupts.
     */
    return (mode != CPSR_MODE_SYS) && (mode != CPSR_MODE_USR);
#elif PROFILE_M
    uint32_t ipsr;
    __asm__ __volatile__("mrs %0, ipsr" : "=r"(ipsr));
    return ipsr != 0;
#endif // PROFILE
}

#if PROFILE_M
static inline bool interrupts_masked(void)
{
    uint32_t primask;
    __asm__("mrs %0, primask" : "=r"(primask));
    return primask != 0;
}
#endif

void rt_syscall_pend(void)
{
#if PROFILE_R
    if (cpsr_mode() == CPSR_MODE_USR)
    {
        __asm__("svc 0");
    }
    else
    {
#if RT_ARCH_ARM_R_VIC_TYPE == VIM_SSI
#define SYS_SSIR1 (*(volatile uint32_t *)0xFFFFFFB0U)
#define SYS_SSIR1_KEY (UINT32_C(0x75) << 8)
        SYS_SSIR1 = SYS_SSIR1_KEY;
        __asm__("dsb" ::: "memory");
        __asm__("isb");
#endif // RT_ARCH_ARM_R_VIC_TYPE
    }

#elif PROFILE_M

#define ICSR (*(volatile uint32_t *)0xE000ED04UL)
#define PENDSVSET (UINT32_C(1) << 28)

    /* If the MPU is enabled, then the current task might be unprivileged,
     * which prevents access to ICSR, so system calls must be invoked with svc.
     * svc can only be used if no interrupt is active and if interrupts are not
     * masked. TODO: is there a more efficient way to check that svc is allowed?
     */
    if (RT_MPU_ENABLE && !rt_interrupt_is_active() && !interrupts_masked())
    {
        __asm__("svc 0");
    }
    else
    {
        ICSR = PENDSVSET;
        __asm__("dsb" ::: "memory");
        __asm__("isb");
    }
#endif // PROFILE
}

void rt_logf(const char *fmt, ...)
{
    (void)fmt;
}

uint32_t rt_cycle(void)
{
#if RT_CYCLE_ENABLE
#if PROFILE_R
    return pmccntr();
#elif PROFILE_M
    return DWT_CYCCNT;
#endif // PROFILE
#else  // RT_CYCLE_ENABLE
    return 0;
#endif
}

#if PROFILE_R && FPU
// A flag indicating whether the active task has an fp context.
volatile bool rt_task_fp_enabled = false;

void rt_task_enable_fp(void)
{
    rt_task_fp_enabled = true;
    /* rt_task_fp_enabled must be set before fpscr. If fpscr is set first and
     * then a context switch occurs, the write to fpscr will be lost. */
    __asm__("dsb" ::: "memory");
    __asm__("vmsr fpscr, %0" : : "r"(0));
}
#endif

void rt_task_drop_privilege(void)
{
#if PROFILE_R
    __asm__("cps %0" : : "i"(CPSR_MODE_USR));
#elif PROFILE_M && RT_MPU_ENABLE
    uint32_t control;
    __asm__("mrs %0, control" : "=r"(control));
    __asm__("msr control, %0" : : "r"(control | CONTROL_NPRIV));
    __asm__("isb");
#endif
}

#if PROFILE_M && __ARM_ARCH == 6
#include "m/atomic-v6.c"
#endif
