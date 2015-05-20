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

// create: make a new task with the given priority, and start it by
// running the code pointed to by the *code pointer.
// The code pointer is assumed to point to a method taking no parameters
// and returning void.
// Returns the TID of the newly created task, or -1 if the priority is invalid,
// or -2 if no more tasks can be created
int create(int priority, void *code) __attribute__((naked));

#define CREATE_INVALID_PRIORITY -1
#define CREATE_INSUFFICIENT_RESOURCES -2

// my_tid: returns the task id of the calling task
int tid(void) __attribute__((naked));

// my_tid: returns the task id of parent of the calling task
// if the parent task has been, or is being destroyed, the value is
// implementation defined
int parent_tid(void) __attribute__((naked));

// pass: yield control flow to the kernel, and have the calling task
// be immediately queued for execution again

void pass(void) __attribute__((naked));

// exit: stop execution of the calling task, and destroy it and all its
// resources.
void exitk(void) __attribute__((naked));

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
