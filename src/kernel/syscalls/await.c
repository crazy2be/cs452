#include "await.h"

#include <kernel.h>
#include <util.h>
#include <least_significant_set_bit.h>

#include "../kassert.h"
#include "../tasks.h"
#include "../drivers/irq.h"
#include "../drivers/timer.h"

static struct task_queue await_queues[EID_NUM_EVENTS] = {};
static int num_tasks_waiting = 0;

// state stored by various hardware interrupts
static unsigned clock_ticks = 0;

void irq_handler(struct task_descriptor *current_task) {
	unsigned long long irq_mask = irq_get_interrupt();
	unsigned irq_mask_lo = irq_mask;
	unsigned irq_mask_hi = irq_mask >> 32;
	unsigned irq;

	if (irq_mask_lo) {
		irq = least_significant_set_bit(irq_mask_lo);
	} else {
		irq = 32 + least_significant_set_bit(irq_mask_hi);
	}

	switch (irq) {
	case IRQ_TIMER:
		tick_timer_clear_interrupt();
		await_event_occurred(EID_TIMER_TICK, ++clock_ticks & 0x7fffffff);
		break;
	default:
		KASSERT(0 && "UNKNOWN INTERRUPT!");
		break;
	}
	task_schedule(current_task);
}

void await_handler(struct task_descriptor *current_task) {
	int eid = syscall_arg(current_task->context, 0);
	if (eid < 0 || eid >= EID_NUM_EVENTS) {
		syscall_set_return(current_task->context, -1);
		return;
	}
	task_queue_push(&await_queues[eid], current_task);
	num_tasks_waiting++;
}

void await_event_occurred(int eid, int data) {
	struct task_descriptor *task;
	while ((task = task_queue_pop(&await_queues[eid]))) {
		num_tasks_waiting--;
		syscall_set_return(task->context, data);
		task_schedule(task);
	}
}

void should_idle_handler(struct task_descriptor *current_task) {
	// Really, the condition for the idle task to exit (and for the kernel to
	// exit) should be "no ready tasks & no await-blocked tasks".
	// However, this is somewhat optimized, since it will only ever be called
	// by the idle task.
	// This means that we already know the ready queue is otherwise empty, so
	// we only need to check for await-blocked tasks.
	int should_idle = num_tasks_waiting;
	syscall_set_return(current_task->context, should_idle);
	task_schedule(current_task);
}
