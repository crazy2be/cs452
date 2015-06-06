#include "msg.h"

#include <util.h>
#include "../tasks.h"

static void dispatch_msg(struct task_descriptor *to, struct task_descriptor *from) {
	// write tid of sender to pointer provided by receiver
	*(unsigned*)syscall_arg(to->context, 0) = from->tid;

	// copy message into buffer
	// truncate it if it won't fit into the receiving buffer
	memcpy((void*) syscall_arg(to->context, 1), (void*) syscall_arg(from->context, 1),
	       MIN((int) syscall_arg(to->context, 2), (int) syscall_arg(from->context, 2)));

	// return sent msg len to the receiver
	syscall_set_return(to->context, syscall_arg(from->context, 2));

	from->state = REPLY_BLK;
	to->state = READY;
	task_schedule(to);
}

void send_handler(struct task_descriptor *current_task) {
	struct user_context *uc = current_task->context;
	int to_tid = syscall_arg(uc, 0);
	// check if the tid exists & is not us
	if (!tid_valid(to_tid) || to_tid == current_task->tid) {
		syscall_set_return(uc, tid_possible(to_tid) ? SEND_INVALID_TID : SEND_IMPOSSIBLE_TID);
		task_schedule(current_task);
		return;
	}
	struct task_descriptor *to_td = task_from_tid(to_tid);

	if (to_td->state == RECV_BLK) {
		// if the task we're sending to is waiting for a reply,
		// immediately exchange the message
		dispatch_msg(to_td, current_task);
	} else {
		// otherwise, queue up on that task, and wait to be processed
		task_queue_push(&to_td->waiting_for_replies, current_task);
		current_task->state = SEND_BLK;
	}
}

void receive_handler(struct task_descriptor *current_task) {
	struct task_descriptor *from_td = task_queue_pop(&current_task->waiting_for_replies);
	if (from_td) {
		dispatch_msg(current_task, from_td);
	} else {
		current_task->state = RECV_BLK;
	}
}

void reply_handler(struct task_descriptor *current_task) {
	struct user_context *recv_context = current_task->context;
	int send_tid = syscall_arg(recv_context, 0);

	// check if the tid exists & is not us
	if (!tid_valid(send_tid) || send_tid == current_task->tid) {
		syscall_set_return(recv_context,
		                   tid_possible(send_tid) ? REPLY_INVALID_TID : REPLY_IMPOSSIBLE_TID);
		task_schedule(current_task);
		return;
	}

	struct task_descriptor *send_td = task_from_tid(send_tid);

	// check that we sending to a task that expects a reply
	if (send_td->state != REPLY_BLK) {
		syscall_set_return(recv_context, REPLY_UNSOLICITED);
		task_schedule(current_task);
		return;
	}

	struct user_context *send_context = send_td->context;

	// copy the reply back
	int send_len = syscall_arg(send_context, 4);
	int recv_len = syscall_arg(recv_context, 2);

	if (send_len < recv_len) {
		syscall_set_return(recv_context, REPLY_TOO_LONG);
		task_schedule(current_task);
		return;
	}

	memcpy((void*) syscall_arg(send_context, 3), (void*) syscall_arg(recv_context, 1), recv_len);

	// return the length of the reply to the sender
	syscall_set_return(send_context, recv_len);
	syscall_set_return(recv_context, REPLY_SUCCESSFUL);

	// queue both the sending and receiving tasks to execute again
	send_td->state = READY;
	task_schedule(send_td);
	task_schedule(current_task);
}
