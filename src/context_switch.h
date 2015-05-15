#pragma once

void* init_task(void *sp, void *pc);
void* exit_kernel(void *sp);
void enter_kernel(void);
