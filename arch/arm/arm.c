#include <rt/context.h>

#include <rt/rt.h>

#include <stdbool.h>

struct gp_context
{
    // Saved by context switch code.
    uint32_t r11, r10, r9, r8, r7, r6, r5, r4;

    // Saved automatically on exception entry.
    uint32_t r0, r1, r2, r3, r12;
    void (*lr)(void);
    void (*pc)(void);
    uint32_t apsr;
};

struct fp_context
{
    float s[16];
    uint32_t fpscr;
    uint32_t : 32;
};

void *rt_context_create(void *stack, size_t stack_size, void (*fn)(void))
{
    void *const stack_end = (char *)stack + stack_size;
    struct gp_context *ctx = stack_end;
    ctx -= 1;
    ctx->apsr = 0;
    ctx->pc = fn;
    ctx->lr = rt_task_exit;
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
    static char idle_stack[sizeof(struct gp_context)];
    static struct rt_task idle;
    rt_task_init(&idle, idle_fn, idle_stack, sizeof idle_stack, "idle", 0);

    // Set the process stack pointer as though the idle task is executing.
    // The first context switch will push a context onto the idle stack.
    // TODO: actually cause the idle task to execute.
    char *idle_stack_end = idle_stack + sizeof idle_stack;
    __asm__ __volatile__("msr psp, %0" : :"r"(idle_stack_end));

    // TODO: Set the priorities of the tick and syscall interrupts.
    // enable them, and trigger the first one.
    rt_sched();
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
