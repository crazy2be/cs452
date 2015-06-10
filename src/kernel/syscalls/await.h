#pragma once
#include "../task_descriptor.h"

void await_init(void);

// called when an IRQ comes in
void irq_handler(struct task_descriptor *current_task);

// called when an await syscall comes in
void await_handler(struct task_descriptor *current_task);

/* void await_event_occurred(int eid, int data); */

// called by idle task to determine if we're still waiting on any events;
// if we are, we shouldn't exit as soon as there are no more ready tasks
void should_idle_handler(struct task_descriptor *current_task);

struct task_descriptor *get_awaiting_task(int eid);
void set_awaiting_task(int eid, struct task_descriptor *td);
void clear_awaiting_task(int eid);

int eid_for_uart(int channel, int is_tx);
void uart_for_eid(int eid, int* channel, int* is_tx);
