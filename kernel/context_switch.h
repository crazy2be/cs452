#pragma once

/** @file */

/**
 * Convenience struct for manipulating saved copy of user context
 * from C code, given a pointer to that user's stack.
 * A pointer to a user context is the saved copy of a user task's
 * stack pointer.
 */
struct user_context {
    unsigned lr;
    unsigned cpsr;
    unsigned pc;
    unsigned r0;
    unsigned r1;
    unsigned r2;
    unsigned r3;
    unsigned r4;
    unsigned r5;
    unsigned r6;
    unsigned r7;
    unsigned r8;
    unsigned r9;
    unsigned r10;
    unsigned r11;
    unsigned r12;
};

// Sugaring to access arguments, agnostic to where they're stored
// This abstraction is zero-cost, since we instruct the compiler to inline this.
// The switch disappears if n is a constant.
static inline unsigned syscall_arg(struct user_context *uc, unsigned n) {
    switch (n) {
    case 0:
        return uc->r0;
    case 1:
        return uc->r1;
    case 2:
        return uc->r2;
    case 3:
        return uc->r3;
    default:
        return ((unsigned*)(uc + 1))[n - 4];
    }
}

static inline void syscall_return(struct user_context *uc, unsigned r) {
    uc->r0 = r;
}

/**
 * Used to return to the kernel from a syscall by the user task,
 * with both the syscall number, and the user's context (which is
 * described by the stack pointer).
 */
struct syscall_context {
    unsigned syscall_num;
    struct user_context *context;
};

/**
 * Leave the kernel, and resume the task with the given stack pointer.
 *
 * This function and its dual `enter_kernel()` are deeply magical.
 * They are implemented in assembly directly, and don't follow the C
 * conventions for control flow.
 * `exit_kernel()` does not return directly after running - it yields
 * control flow to the user task.
 * The user task is assumed to make a syscall (or later, receive an interrupt)
 * after some time, which will call `enter_kernel()`.
 * `enter_kernel()` returns to the same place that `exit_kernel()` was called
 * from, with the syscall_context.
 * From the kernel's perspective, it looks like `exit_kernel()` is
 * returning with those arguments.
 *
 * To summarize, `exit_kernel()` is called only by the kernel, and to it,
 * it looks like it returns like a normal function with the returns documented
 * here.
 * Similarly, `enter_kernel()` is called only be the user tasks, and to them,
 * it looks like it returns like a normal function with no arguments.
 * In fact, control flows back and forth between the kernel and user tasks,
 * and contexts are saved and restored so that this is transparent to the callers.
 *
 * @param context: A pointer to the user task's context. This is just
 * a copy of their stack pointer after saving their registers on the stack,
 * in the format specified by `struct user_context`.
 *
 * @return The syscall context, which contains the syscall number which the
 * user task returned to the kernel with, and the context of the task
 * when it returned.
 *
 * @see struct user_context
 * @see exit_kernel()
 */
struct syscall_context exit_kernel(struct user_context *context);

/**
 * Leave the user task, and context switch into the kernel.
 *
 * This function is not called directly by the user tasks - it's branched
 * to after the user raises a software interrupt.o
 *
 * While `exit_kernel()` does not itself return any values or pass any parameters,
 * the kernel can read and write to the user context while in kernel mode.
 * So it can read any arguments taken from the context, and write the returns
 * back into the context.
 * When the user task's context is restored, the modified context is restored
 * into the registers, and it as though `enter_kernel()` returns values.
 *
 * This function is deeply magical and does strange things with control flow.
 * This is better documented on `enter_kernel()`
 *
 * @see enter_kernel()
 */
void enter_kernel(void);
