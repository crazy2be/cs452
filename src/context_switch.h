#pragma once

void* init_task_stack(void *sp, void *pc);
void* exit_kernel(void *sp);
void enter_kernel(void);
