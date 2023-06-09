#ifndef RT_TASK_H
#define RT_TASK_H

#include <muntos/context.h>
#include <muntos/cycle.h>
#include <muntos/list.h>
#include <muntos/mpu.h>
#include <muntos/stack.h>
#include <muntos/syscall.h>

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
 * Yield the core to another task of the same priority. If the current task is
 * still the highest priority, it will continue executing.
 */
void rt_task_yield(void);

/*
 * Exit from the current task. This will be called automatically when a task
 * function returns.
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
 * task executes any floating-point instructions.
 */
void rt_task_enable_fp(void);

/*
 * On architectures that have privilege levels, make the current task
 * unprivileged.
 */
void rt_task_drop_privilege(void);

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
#if RT_MPU_ENABLE
    struct rt_mpu_config mpu_config;
#endif
    unsigned long wake_tick;
    struct rt_syscall_record record;
    const char *name;
    unsigned priority;
    enum rt_task_state state;
};

#define RT_TASK_INIT(name_, name_str, priority_)                               \
    {                                                                          \
        .list = RT_LIST_INIT(name_.list),                                      \
        .sleep_list = RT_LIST_INIT(name_.sleep_list),                          \
        .record.syscall = RT_SYSCALL_TASK_READY, .name = (name_str),           \
        .priority = (priority_),                                               \
    }

#define RT_TASK(fn, stack_size, priority_)                                     \
    do                                                                         \
    {                                                                          \
        RT_STACK(fn##_task_stack, stack_size);                                 \
        static struct rt_task fn##_task =                                      \
            RT_TASK_INIT(fn##_task, #fn, priority_);                           \
        fn##_task.ctx =                                                        \
            rt_context_create((fn), fn##_task_stack, sizeof fn##_task_stack);  \
        rt_mpu_config_init(&fn##_task.mpu_config);                             \
        rt_mpu_config_set(&fn##_task.mpu_config, RT_MPU_TASK_REGION_START_ID,  \
                          (uintptr_t)fn##_task_stack, stack_size,              \
                          RT_MPU_STACK_ATTR);                                  \
        rt_syscall(&fn##_task.record);                                         \
    } while (0)

#define RT_TASK_ARG(fn, arg, stack_size, priority_)                            \
    do                                                                         \
    {                                                                          \
        RT_STACK(fn##_task_stack, stack_size);                                 \
        static struct rt_task fn##_task =                                      \
            RT_TASK_INIT(fn##_task, #fn "(" #arg ")", priority_);              \
        fn##_task.ctx = rt_context_create_arg((fn), (arg), fn##_task_stack,    \
                                              sizeof fn##_task_stack);         \
        rt_mpu_config_init(&fn##_task.mpu_config);                             \
        rt_mpu_config_set(&fn##_task.mpu_config, RT_MPU_TASK_REGION_START_ID,  \
                          (uintptr_t)fn##_task_stack, stack_size,              \
                          RT_MPU_STACK_ATTR);                                  \
        rt_syscall(&fn##_task.record);                                         \
    } while (0)

#endif /* RT_TASK_H */
