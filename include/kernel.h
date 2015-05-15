#pragma once

/*
// create: make a new task with the given priority, and start it by
// running the code pointed to by the *code pointer.
// The code pointer is assumed to point to a method taking no parameters
// and returning void.
int create(int priority, void *code);

// my_tid: returns the task id of the calling task
int tid();

// my_tid: returns the task id of parent of the calling task
// if the parent task has been, or is being destroyed, the value is
// implementation defined
int parent_tid();
*/

// pass: yield control flow to the kernel, and have the calling task
// be immediately queued for execution again
void pass(void);

/*
// exit: stop execution of the calling task, and destroy it and all its
// resources.
void exit(void);

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
    exit();
}
*/