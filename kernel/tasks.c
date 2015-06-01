#include "tasks.h"

#include "util.h"

static struct task_collection tasks;
static struct priority_task_queue queue;

void tasks_init(void) {
    tasks.next_tid = 0;
	extern int stack_top;
    tasks.memory_alloc = (void*) &stack_top + USER_STACK_SIZE;

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
    struct task_descriptor *task = &tasks.task_buf[tasks.next_tid];
	*task = (struct task_descriptor){
		.tid = tasks.next_tid++,
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
    uc->cpsr = cpsr & 0xfffffff0;

    task->context = uc;

    return task;
}

int tasks_full() {
	return tasks.next_tid > MAX_TID;
}
struct task_descriptor *task_from_tid(int tid) {
	KASSERT(tid >= 0 && tid < tasks.next_tid);
	return &tasks.task_buf[tid];
}

void task_schedule(struct task_descriptor *task) {
	priority_task_queue_push(&queue, task);
}

struct task_descriptor *task_next_scheduled() {
	return priority_task_queue_pop(&queue);
}

int tid_valid(int tid) {
	return tid >= 0 && tid < tasks.next_tid
		&& tasks.task_buf[tid].state != ZOMBIE;
}
int tid_possible(int tid) { return tid >= 0 && tid <= MAX_TID; }
