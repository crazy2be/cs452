#include "task_descriptor.h"

void await_handler(struct task_descriptor *current_task);
void await_event_occurred(int eid, int data);
int await_tasks_waiting();
