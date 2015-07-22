#include "task_management.h"

#include "../tasks.h"
#include "../drivers/timer.h"
#include "../kassert.h"

// Implementation of syscalls
//
// All that any of these functions should do is to retrieve arguments from
// the context of the calling stack, return results to the context,
// and perform validation.
// There should be very little application logic in this code.

void create_handler(struct task_descriptor *current_task) {
	struct user_context *uc = current_task->context;
	int priority = (int) syscall_arg(uc, 0);
	void *code = (void*) syscall_arg(uc, 1);
	int result;

	if (priority < PRIORITY_MAX || priority > PRIORITY_MIN) {
		result = CREATE_INVALID_PRIORITY;
	} else if (tasks_full()) {
		result = CREATE_INSUFFICIENT_RESOURCES;
	} else {
		struct task_descriptor *new_task = task_create(code, priority, current_task->tid);
		task_schedule(new_task);
		result = new_task->tid;
	}

	syscall_set_return(uc, result);
	task_schedule(current_task);
}

void pass_handler(struct task_descriptor *current_task) {
	// no-op, just reschedule.
	task_schedule(current_task);
}

void exit_handler(struct task_descriptor *current_task) {
	// TODO: we need to signal to tasks trying to send/receive to this
	// that the send/receive has failed
	// Should we signal when the task exits after receiving, but before replying?
	// some other task might still reply to the message, according to the spec
	task_kill(current_task);
	struct task_descriptor *td;
	while ((td = task_queue_pop(&current_task->waiting_for_replies))) {
		// signal to the sending task that the send failed
		syscall_set_return(td->context, SEND_INCOMPLETE);
		td->state = READY;
		task_schedule(td);
	}
	// drop the current task off the queue
}

void tid_handler(struct task_descriptor *current_task) {
	syscall_set_return(current_task->context, current_task->tid);
	task_schedule(current_task);
}

void parent_tid_handler(struct task_descriptor *current_task) {
	syscall_set_return(current_task->context, current_task->parent_tid);
	task_schedule(current_task);
}

void idle_permille_handler(struct task_descriptor *current_task, unsigned ts_start) {
	const struct task_descriptor *idle_td = task_from_tid(0);
	KASSERT(idle_td != NULL);
	unsigned idle = idle_td->user_time_useconds;
	unsigned total = debug_timer_useconds() - ts_start;

	static unsigned last_idle = 0, last_total = 0;
	KASSERT(total >= last_total);
	KASSERT(idle >= last_idle);
	last_total = total;
	last_idle = idle;

	unsigned permille = idle / (total / 1000);
	KASSERT(permille <= 1000);
	(void) permille;

	syscall_set_return(current_task->context, permille);
	task_schedule(current_task);
}

void task_info_handler(struct task_descriptor *current_task) {
	struct user_context *uc = current_task->context;
	int subject_tid = (int) syscall_arg(uc, 0);
	struct task_info *info_out = (struct task_info*) syscall_arg(uc, 1);

	if (!tid_valid(subject_tid)) {
		syscall_set_return(uc, TASK_STATE_INVALID_TID);
	} else {
		const struct task_descriptor *subject_td = task_from_tid(subject_tid);
		info_out->state = subject_td->state;
		syscall_set_return(uc, TASK_STATE_SUCCESS);
	}

	task_schedule(current_task);
}
