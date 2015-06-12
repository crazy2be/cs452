#include "await.h"

#include <kernel.h>
#include <util.h>
#include <io.h>

#include "../kassert.h"
#include "../tasks.h"
#include "../drivers/irq.h"
#include "../drivers/timer.h"
#include "await_io.h"

static struct task_descriptor *await_blocked_tasks[EID_NUM_EVENTS] = {};
static int num_tasks_waiting = 0;

struct task_descriptor *get_awaiting_task(int eid) {
	return await_blocked_tasks[eid];
}

void set_awaiting_task(int eid, struct task_descriptor *td) {
	KASSERT(await_blocked_tasks[eid] == NULL);
	num_tasks_waiting++;
	await_blocked_tasks[eid] = td;
}

void clear_awaiting_task(int eid) {
	KASSERT(await_blocked_tasks[eid] != NULL);
	num_tasks_waiting--;
	await_blocked_tasks[eid] = NULL;
}

void await_event_occurred(int eid, int data) {
	struct task_descriptor *task = get_awaiting_task(eid);
	if (task) {
		clear_awaiting_task(eid);
		syscall_set_return(task->context, data);
		task_schedule(task);
	}
}

// state stored by various hardware interrupts
static unsigned clock_ticks = 0;

void await_init(void) {
	io_irq_init();
}

void irq_handler(struct task_descriptor *current_task) {
	unsigned long long irq_mask = irq_get_interrupt();
	unsigned irq_mask_lo = irq_mask;
	unsigned irq_mask_hi = irq_mask >> 32;

	/* extern void uart_print_ctrl(int); */
	/* if (clock_ticks % 100 == 0) { */
	/* 	printf("As of tick %d...\r\n", clock_ticks); */
	/* 	uart_print_ctrl(COM1); */
	/* 	uart_print_ctrl(COM2); */
	/* 	printf("\r\n"); */
	/* } */

	if (IRQ_TEST(IRQ_TIMER, irq_mask_lo, irq_mask_hi)) {
		tick_timer_clear_interrupt();
		await_event_occurred(EID_TIMER_TICK, ++clock_ticks & 0x7fffffff);
	} else if (IRQ_TEST(IRQ_COM1, irq_mask_lo, irq_mask_hi)) {
		/* uart_print_ctrl(COM1); */
		io_irq_handler(COM1);
	} else if (IRQ_TEST(IRQ_COM2, irq_mask_lo, irq_mask_hi)) {
		/* uart_print_ctrl(COM1); */
		io_irq_handler(COM2);
	} else {
		KASSERT(0 && "UNKNOWN INTERRUPT!");
	}

	task_schedule(current_task);
}

int eid_for_uart(int channel, int is_tx) {
	return EID_COM1_READ + is_tx + 2*channel;
}
void uart_for_eid(int eid, int* channel, int* is_tx) {
	*channel = (eid - EID_COM1_READ) / 2;
	*is_tx = (eid - EID_COM1_READ) % 2;
}

void await_handler(struct task_descriptor *current_task) {
	int eid = syscall_arg(current_task->context, 0);
	if (eid < 0 || eid >= EID_NUM_EVENTS) {
		syscall_set_return(current_task->context, AWAIT_UNKNOWN_EVENT);
		task_schedule(current_task);
		return;
	}

	if (eid >= EID_COM1_READ && eid <= EID_COM2_WRITE) {
		int channel, is_tx;
		uart_for_eid(eid, &channel, &is_tx);
		io_irq_mask_add(channel, is_tx);
	}

	if (get_awaiting_task(eid)) {
		// for our purposes, there is never a case where we want to have multiple
		// tasks awaiting the same event, so we just disallow it
		syscall_set_return(current_task->context, AWAIT_MULTIPLE_WAITERS);
		task_schedule(current_task);
	} else {
		set_awaiting_task(eid, current_task);
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
