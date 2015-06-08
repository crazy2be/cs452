#pragma once
#include "../task_descriptor.h"

void irq_handler(struct task_descriptor *current_task);
void await_handler(struct task_descriptor *current_task);
void await_event_occurred(int eid, int data);
void should_idle_handler(struct task_descriptor *current_task);
