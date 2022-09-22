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

void rt_start(void)
{
    __attribute__((aligned(8))) static char idle_stack[72];

    // TODO: Set the priorities of the tick and syscall interrupts and enable
    // them.

    __asm__ __volatile__(
        // Set the process stack pointer to the top of the idle task stack.
        "msr psp, %0\n"
        // Switch to the process stack pointer and drop privileges.
        "mov r0, 3\n"
        "msr control, r0\n"
        "isb\n"
        :
        : "r"(&idle_stack[sizeof idle_stack]));

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
