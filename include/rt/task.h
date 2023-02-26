#ifndef RT_TASK_H
#define RT_TASK_H

#include <rt/context.h>
#include <rt/cycle.h>
#include <rt/list.h>
#include <rt/syscall.h>

#include <stddef.h>
#include <stdint.h>

#ifndef RT_TASK_ENABLE_CYCLE
#define RT_TASK_ENABLE_CYCLE 0
#endif

#if RT_TASK_ENABLE_CYCLE && !RT_CYCLE_ENABLE
#error "To use task cycle counts, the cycle counter must be enabled."
#endif

struct rt_task;

/*
 * Initialize a task that runs fn() on the given stack, and make it runnable.
 * Must be called before rt_start().
 */
void rt_task_init(struct rt_task *task, void (*fn)(void), const char *name,
                  unsigned priority, void *stack, size_t stack_size);

/*
 * Initialize a task that runs fn(arg) on the given stack, and make it runnable.
 * Must be called before rt_start().
 */
void rt_task_init_arg(struct rt_task *task, void (*fn)(uintptr_t),
                      uintptr_t arg, const char *name, unsigned priority,
                      void *stack, size_t stack_size);

/*
 * Exit from the current task. This should be called automatically when a
 * task function returns.
 */
void rt_task_exit(void);

/*
 * Get the name of the current task.
 */
const char *rt_task_name(void);

/*
 * Get the current task.
 */
struct rt_task *rt_task_self(void);

/*
 * On some floating-point capable architectures, it is necessary for each task
 * to indicate that it needs a floating-point context; this function is only
 * implemented for those architectures (currently just Arm R-Profile). On these
 * architectures, rt_task_enable_fp must be called in each task before that
 * task executes any floating-point code.
 */
void rt_task_enable_fp(void);

enum rt_task_state
{
    RT_TASK_STATE_RUNNING,
    RT_TASK_STATE_READY,
    RT_TASK_STATE_BLOCKED,
    RT_TASK_STATE_BLOCKED_TIMEOUT,
    RT_TASK_STATE_ASLEEP,
    RT_TASK_STATE_EXITED,
};

struct rt_task
{
    struct rt_list list;
    struct rt_list sleep_list;
#if RT_TASK_ENABLE_CYCLE
    uint64_t total_cycles;
    uint32_t start_cycle;
#endif
    void *ctx;
    unsigned long wake_tick;
    struct rt_syscall_record record;
    const char *name;
    unsigned priority;
    enum rt_task_state state;
};

#define RT_TASK(fn, stack, priority_)                                          \
    do                                                                         \
    {                                                                          \
        static struct rt_task fn##_task = {                                    \
            .list = RT_LIST_INIT(fn##_task.list),                              \
            .sleep_list = RT_LIST_INIT(fn##_task.sleep_list),                  \
            .record.syscall = RT_SYSCALL_TASK_READY,                           \
            .name = #fn,                                                       \
            .priority = (priority_),                                           \
        };                                                                     \
        fn##_task.ctx = rt_context_create((fn), (stack), sizeof(stack));       \
        rt_syscall(&fn##_task.record);                                         \
    } while (0)

#define RT_TASK_ARG(fn, arg, stack, priority_)                                 \
    do                                                                         \
    {                                                                          \
        static struct rt_task fn##_task = {                                    \
            .list = RT_LIST_INIT(fn##_task.list),                              \
            .sleep_list = RT_LIST_INIT(fn##_task.sleep_list),                  \
            .record.syscall = RT_SYSCALL_TASK_READY,                           \
            .name = #fn "(" #arg ")",                                          \
            .priority = (priority_),                                           \
        };                                                                     \
        fn##_task.ctx =                                                        \
            rt_context_create_arg((fn), (arg), (stack), sizeof(stack));        \
        rt_syscall(&fn##_task.record);                                         \
    } while (0)

#endif /* RT_TASK_H */
