#pragma once

#include "../task_descriptor.h"

void create_handler(struct task_descriptor *current_task);
void pass_handler(struct task_descriptor *current_task);
void exit_handler(struct task_descriptor *current_task);
void tid_handler(struct task_descriptor *current_task);
void parent_tid_handler(struct task_descriptor *current_task);
void rand_handler(struct task_descriptor *current_task);
void idle_permille_handler(struct task_descriptor *current_task, unsigned ts_start);
