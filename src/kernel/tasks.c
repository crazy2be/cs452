#include "tasks.h"

#include <util.h>
#include "kassert.h"

static struct task_collection tasks;
static struct task_queue free_tds;
static struct priority_task_queue queue;

void tasks_init(void) {
	memset(&tasks, sizeof(tasks), 0);
	extern int stack_top;
	tasks.memory_alloc = (void*) &stack_top + USER_STACK_SIZE;

	task_queue_init(&free_tds);
	for (int i = 0; i < NUM_TID; i++) {
		tasks.task_buf[i].tid = i - NUM_TID; // Is this the best way to do this?
		task_queue_push(&free_tds, &tasks.task_buf[i]);
	}
	priority_task_queue_init(&queue);
}

/**
 * Internal kernel utility for creating a task, and allocating the necessary resources
 *
 * Allocates space on the task buffer as a side-effect, and increments the count
 * of created tasks.
 * It initializes the task descriptor for the newly created task.
 *
 * It also allocates memory for the tasks stack, and initializes it.
 * After the stack has been initalized, in can be context switched to normally by
 * using `exit_kernel()`, and will start executing at the entrypoint.
 *
 * Notedly, this does *not* schedule the newly created task for execution.
 */
struct task_descriptor *task_create(void *entrypoint, int priority, int parent_tid) {
	void *sp = tasks.memory_alloc;
	tasks.memory_alloc += USER_STACK_SIZE;
	struct task_descriptor *task = task_queue_pop(&free_tds);
	*task = (struct task_descriptor) {
		.tid = task->tid + NUM_TID,
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

int tasks_full() {
	return task_queue_empty(&free_tds);
}
struct task_descriptor *task_from_tid(int tid) {
	KASSERT(tid >= 0);
	struct task_descriptor *task = &tasks.task_buf[tid % NUM_TID];
	KASSERT(task->tid == tid); // TODO: Better error handling?
	return &tasks.task_buf[tid % NUM_TID];
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
	return tasks.next_tid;
}
