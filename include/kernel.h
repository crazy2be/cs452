#pragma once

#include <constants.h>

#define SYSCALL_PASS 0
#define SYSCALL_EXIT 1
#define SYSCALL_CREATE 2
#define SYSCALL_TID 3
#define SYSCALL_PARENT_TID 4

// stringization macros
#define STR1(x) #x
#define SYSCALL(x) "swi " STR1(x) "\n\tbx lr"

/** @file */

/**
 * Make a new task with the given priority and code.
 * @param priority: The highest priority is zero, and decreases as the
 * priority number increases.
 * @param code: Assumed to be a function pointer like: void (*f)(void).
 * This argument is not typed as an actual function pointer because the spec says
 * to do it this way.
 * @return The TID of the newly created task, CREATE_INVALID_PRIORITY if the priority
 * is invalid, or CREATE_INSUFFICIENT_RESOURCES if there are no more task descriptors
 * to be allocated.
 */
int create(int priority, void *code) __attribute__((naked));

#define CREATE_INVALID_PRIORITY -1
#define CREATE_INSUFFICIENT_RESOURCES -2

/**
 * @return The task id of the calling task
 */
int tid(void) __attribute__((naked));

/**
 * @return The task id of the parent of the calling task.
 * This is implementation dependant if the parent has exited.
 * In our implementation, the return is unchanged when the parent exits,
 * and points to a process which is a zombie.
 * Because TIDs are never reused, this can't cause confusion even if
 * the task resources are reused.
 */
int parent_tid(void) __attribute__((naked));

/**
 * Yield control flow to the kernel or other tasks.
 * This is a no-op, as far as the caller is concerned.
 */
void pass(void) __attribute__((naked));

/**
 * Stop execution of the calling task, and destroy it and all its
 * resources.
 */
void exitk(void) __attribute__((naked, noreturn));

// aliases for the above functions,
// using the naming convention specified by the course

static inline int Create(int priority, void *code) {
    return create(priority, code);
}

static inline int MyTid() {
    return tid();
}

static inline int MyParentTid() {
    return parent_tid();
}

static inline void Pass(void) {
    pass();
}

static inline void Exit(void) {
    exitk();
}
