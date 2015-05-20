#pragma once

// convenience struct for manipulating saved copy of user context
// from c code, given a pointer to that user's stack
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

struct syscall_context {
    unsigned syscall_num;
    void *stack_pointer;
};
struct syscall_context exit_kernel(void *sp);
void enter_kernel(void);
