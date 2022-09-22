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
    uint32_t apsr;
};

void *rt_context_create(void *stack, size_t stack_size, void (*fn)(void))
{
    void *const stack_end = (char *)stack + stack_size;
    struct gp_context *ctx = stack_end;
    ctx -= 1;
    ctx->apsr = 0;
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
    static char idle_stack[72];

    // TODO: Set the priorities of the tick and syscall interrupts and enable them.

    // Set the process stack pointer to the top of the idle task stack.
    __asm__ __volatile__(
            "msr psp, %0\n" // Set the process stack pointer to the idle task stack.
            "mov r0, 3\n" // Switch to the process stack pointer and drop privileges.
            "msr control, r0\n"
            "isb\n"
            : :"r"(&idle_stack[sizeof idle_stack]));

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
    // TODO: is using pendSV instead within ISRs necessary?
    // ICSR->PENDSVSET = true;
    __asm__ __volatile__("svc 0");
}
