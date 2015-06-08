#pragma once
#include "../task_descriptor.h"

void send_handler(struct task_descriptor *current_task);
void receive_handler(struct task_descriptor *current_task);
void reply_handler(struct task_descriptor *current_task);
