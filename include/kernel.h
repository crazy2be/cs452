#pragma once

#define SYSCALL_PASS 0
#define SYSCALL_EXIT 1
#define SYSCALL_CREATE 2
#define SYSCALL_TID 3
#define SYSCALL_PARENT_TID 4

// stringization macros
#define STR1(x) #x
#define SYSCALL(x) "swi " STR1(x)

// create: make a new task with the given priority, and start it by
// running the code pointed to by the *code pointer.
// The code pointer is assumed to point to a method taking no parameters
// and returning void.
// Returns the TID of the newly created task, or -1 if the priority is invalid,
// or -2 if no more tasks can be created
inline int create(int priority, void *code) __attribute__((always_inline));
inline int create(int priority, void *code) {
    register int tid __asm__("r0");
    __asm__ __volatile__ (
        "mov r0, %0"
        "mov r1, %1"
        SYSCALL(SYSCALL_CREATE)
        : "=r" (tid)
    );
    return tid;
}

// my_tid: returns the task id of the calling task
inline int tid(void) __attribute__((always_inline));
inline int tid(void) {
    register int tid __asm__("r0");
    __asm__ __volatile__ (
        SYSCALL(SYSCALL_TID)
        : "=r" (tid)
    );
    return tid;
}

// my_tid: returns the task id of parent of the calling task
// if the parent task has been, or is being destroyed, the value is
// implementation defined
inline int parent_tid(void) __attribute__((always_inline));
inline int parent_tid(void) {
    register int tid __asm__("r0");
    __asm__ __volatile__ (
        SYSCALL(SYSCALL_PARENT_TID)
        : "=r" (tid)
    );
    return tid;
}

// pass: yield control flow to the kernel, and have the calling task
// be immediately queued for execution again

inline void pass(void) __attribute__((always_inline));
inline void pass(void) {
    __asm__ __volatile__ (SYSCALL(SYSCALL_PASS));
}

// exit: stop execution of the calling task, and destroy it and all its
// resources.
inline void exitk(void) __attribute__((always_inline));
inline void exitk(void) {
    __asm__ __volatile__ (SYSCALL(SYSCALL_EXIT));
}

/*
// aliases for the above functions,
// using the naming convention specified by the course
inline int Create(int priority, void *code) {
    return create(priority, code);
}

inline int MyTid() {
    return tid();
}

inline int MyParentTid() {
    return parent_tid();
}

inline void Pass(void) {
    pass();
}

inline void Exit(void) {
    exitk();
}
*/
