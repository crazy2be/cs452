#pragma once

void* init_task_stack(void *sp, void *pc);
struct syscall_context {
    unsigned syscall_num;
    void *stack_pointer;
};
struct syscall_context exit_kernel(void *sp);
void enter_kernel(void);
