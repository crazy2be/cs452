#pragma once
#include "task_descriptor.h"

void tasks_init(void);

int tasks_full(); // Space for more tasks?
// This does NOT schedule the newly created task for execution.
struct task_descriptor *task_create(void *entrypoint, int priority, int parent_tid);
void task_kill(struct task_descriptor *task);

// Schedule a task for potential future execution.
void task_schedule(struct task_descriptor *task);
struct task_descriptor *task_next_scheduled();

int tid_valid(int tid); // Does TID refer to a living task?
int tid_possible(int tid);
struct task_descriptor *task_from_tid(int tid);

void tasks_print_runtime(int total_runtime_us);
