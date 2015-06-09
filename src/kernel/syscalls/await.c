#include "await.h"

#include <kernel.h>
#include <util.h>
#include <io.h>

#include "../kassert.h"
#include "../tasks.h"
#include "../drivers/irq.h"
#include "../drivers/timer.h"
#include "await_irq.h"

static struct task_queue await_queues[EID_NUM_EVENTS] = {};
static int num_tasks_waiting = 0;

// state stored by various hardware interrupts
static unsigned clock_ticks = 0;

void await_init(void) {
	io_irq_init();
}

void irq_handler(struct task_descriptor *current_task) {
	unsigned long long irq_mask = irq_get_interrupt();
	unsigned irq_mask_lo = irq_mask;
	unsigned irq_mask_hi = irq_mask >> 32;

	if (IRQ_TEST(IRQ_TIMER, irq_mask_lo, irq_mask_hi)) {
		tick_timer_clear_interrupt();
		await_event_occurred(EID_TIMER_TICK, ++clock_ticks & 0x7fffffff);
	} else if (IRQ_TEST(IRQ_COM1, irq_mask_lo, irq_mask_hi)) {
		io_irq_handler(COM1);
	} else if (IRQ_TEST(IRQ_COM2, irq_mask_lo, irq_mask_hi)) {
		io_irq_handler(COM2);
	} else {
		KASSERT(0 && "UNKNOWN INTERRUPT!");
	}

	task_schedule(current_task);
}

void await_handler(struct task_descriptor *current_task) {
	int eid = syscall_arg(current_task->context, 0);
	if (eid < 0 || eid >= EID_NUM_EVENTS) {
		syscall_set_return(current_task->context, -1);
		return;
	}

	int immediate_return;

	switch (eid) {
	case EID_COM1_READ:
		immediate_return = io_irq_check_for_pending_rx(COM1, current_task);
		break;
	case EID_COM1_WRITE:
		immediate_return = io_irq_check_for_pending_tx(COM1, current_task);
		break;
	case EID_COM2_READ:
		immediate_return = io_irq_check_for_pending_rx(COM2, current_task);
		break;
	case EID_COM2_WRITE:
		immediate_return = io_irq_check_for_pending_tx(COM2, current_task);
		break;
	default:
		immediate_return = 1;
	}

	if (!immediate_return) {
		printf("Queuing task on the await queue" EOL);
		task_queue_push(&await_queues[eid], current_task);
		num_tasks_waiting++;
	}
}

struct task_descriptor *get_awaiting_task(int eid) {
	struct task_descriptor *td = task_queue_pop(&await_queues[eid]);
	if (td) num_tasks_waiting--;
	return td;
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
