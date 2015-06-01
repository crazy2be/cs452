#include "await.h"

#include "tasks.h"
#include "kernel.h"
#include "util.h"

static struct task_queue await_queues[NUM_AWAIT_EVENTS] = {};
static int num_tasks_waiting = 0;

static inline int iaq_from_eid(int eid) { return ((eid & 0xFFFF0000) >> 16) - 1; }
void await_handler(struct task_descriptor *current_task) {
	int eid = syscall_arg(current_task->context, 0);
	int iaq = iaq_from_eid(eid);
	printf("Pushing to %d\n", iaq);
	task_queue_push(&await_queues[iaq], current_task);
	num_tasks_waiting++;
}

void await_event_occurred(int eid, int data) {
	int iaq = iaq_from_eid(eid);
	struct task_descriptor *task;
	while ((task = task_queue_pop(&await_queues[iaq]))) {
		num_tasks_waiting--;
		syscall_set_return(task->context, data);
		task_schedule(task);
	}
}

int await_tasks_waiting() { return num_tasks_waiting; }
