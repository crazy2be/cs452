#include "tasks.h"

#include <util.h>
#include "kassert.h"

static struct task_collection tasks;
static struct task_queue free_tds;
static struct priority_task_queue queue;

void tasks_init(void) {
	memset(&tasks, sizeof(tasks), 0);
	task_queue_init(&free_tds);
	for (int i = 0; i < NUM_TID; i++) {
		tasks.task_buf[i].tid = i - NUM_TID; // Is this the best way to do this?
		task_queue_push(&free_tds, &tasks.task_buf[i]);
	}
	priority_task_queue_init(&queue);
}

int tasks_full() {
	return task_queue_empty(&free_tds);
}
// Does *not* schedule the newly created task for execution.
struct task_descriptor *task_create(void *entrypoint, int priority, int parent_tid) {
	struct task_descriptor *task = task_queue_pop(&free_tds);
	// Syscalls should check tasks_full() first if they want to handle this
	// case gracefully.
	KASSERT(task);
	KFASSERT(task->state == DEAD, "%d", task->state);
	int tid = task->tid + NUM_TID;
	// TODO: Are stacks ascending or descending in memory? One of those makes
	// this wrong (although it seems to _work_, but it might break other things)
	void *sp = tasks.stacks[tid%NUM_TID + 1];
	memset(sp - sizeof(tasks.stacks[0]), 0, sizeof(tasks.stacks[0]));
	*task = (struct task_descriptor) {
		.tid = tid,
		.parent_tid = parent_tid,
		.priority = priority,
		.state = READY,
	};

	struct user_context *uc = ((struct user_context*) sp) - 1;
	uc->pc = (unsigned) entrypoint;
	uc->lr = (unsigned) &exitk;
	// leave most everything else uninitialized
	uc->r12 = (unsigned) sp;
	unsigned cpsr;
	__asm__ ("mrs %0, cpsr" : "=r" (cpsr));
	cpsr &= 0xfffffff0; // set mode to user mode
	/* cpsr |= 0x80; // turn interrupts on for this task */
	cpsr &= ~0x80; // deassert the I bit to turn on interrupts
	uc->cpsr = cpsr;

	task->context = uc;

	task->user_time_useconds = 0;

	return task;
}
// WARNING: This doesn't look through the pending queues for the given task,
// so killing a task which is already scheduled to execute will not work out
// well.
// TODO: Should we empty message queues here as well?
void task_kill(struct task_descriptor *task) {
	task->state = DEAD;
	task_queue_push(&free_tds, task);
}

void task_schedule(struct task_descriptor *task) {
	priority_task_queue_push(&queue, task);
}
struct task_descriptor *task_next_scheduled() {
	return priority_task_queue_pop(&queue);
}

int tid_valid(int tid) {
	return tid >= 0
			&& tasks.task_buf[tid % NUM_TID].tid == tid
			&& tasks.task_buf[tid % NUM_TID].state != DEAD;
}
int tid_possible(int tid) {
	return 1; // TODO: What should this be?
}
int tid_next(void) {
	return 256; // TODO: Should be task_queue_top()->tid + NUM_TID or something?
}
struct task_descriptor *task_from_tid(int tid) {
	KASSERT(tid >= 0);
	struct task_descriptor *task = &tasks.task_buf[tid % NUM_TID];
	KASSERT(task->tid == tid); // Call tid_valid first to handle this gracefully.
	return &tasks.task_buf[tid % NUM_TID];
}
