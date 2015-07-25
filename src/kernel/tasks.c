#include "tasks.h"

#include <util.h>
#include <prng.h>
#include "kassert.h"
#include "stack.h"

static struct task_descriptor tasks[NUM_TD];
static struct task_queue free_tds;
static struct priority_task_queue queue;

static const unsigned stack_canary[4] = { 0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef };

void tasks_init(void) {
	memset(&tasks, 0, sizeof(tasks));

#ifdef QEMU
	struct prng prng;
	prng_init(&prng, STACK_SEED);
	for (int i = 0; i < NUM_TD; i++) {
		prng_gen_buf(&prng, stacks[i], USER_STACK_SIZE);
	}
#endif

	task_queue_init(&free_tds);
	for (int i = 0; i < NUM_TD; i++) {
		tasks[i].tid = i - NUM_TD; // Is this the best way to do this?
		task_queue_push(&free_tds, &tasks[i]);

		// setup stack canary when all the tasks are initialized
		void *sp = stacks[i];
		memcpy(sp, stack_canary, sizeof(stack_canary));
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

	int tid = task->tid + NUM_TD;
	KFASSERT(tid_possible(tid), "%d", tid);
	KASSERT((tid % NUM_TD) == (task - tasks));

	// Full descending stack, so we actually initialize the sp to point one
	// element past the end of the stack allocated for the task, where the
	// stack will grow "backwards" from there (never touching that space).
	void *sp = stacks[tid % NUM_TD + 1];

	unsigned cpsr;
	__asm__ ("mrs %0, cpsr" : "=r" (cpsr));
	cpsr &= 0xfffffff0; // set mode to user mode
	/* cpsr |= 0x80; // turn interrupts on for this task */
	cpsr &= ~0x80; // deassert the I bit to turn on interrupts

	struct user_context *uc = ((struct user_context*) sp) - 1;
	*uc = (struct user_context) {
		.pc = (unsigned) entrypoint,
		 .lr = (unsigned) &exitk,
		  // leave most everything else uninitialized
		  .r12 = (unsigned) sp,
		   .cpsr = cpsr,
	};

	*task = (struct task_descriptor) {
		.tid = tid,
		 .parent_tid = parent_tid,
		  .priority = priority,
		   .state = READY,
		    .context = uc,
		     .user_time_useconds = task->user_time_useconds, // Preserve
	};

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
	       && tasks[tid % NUM_TD].tid == tid
	       && tasks[tid % NUM_TD].state != DEAD;
}
int tid_possible(int tid) {
	return tid >= 0;
}
struct task_descriptor *task_from_tid(int tid) {
	KASSERT(tid >= 0);
	struct task_descriptor *task = &tasks[tid % NUM_TD];
	KASSERT(task->tid == tid); // Call tid_valid first to handle this gracefully.
	return task;
}

void tasks_print_runtime(int total_runtime_us) {
	int tasks_runtime_us = 0;
	for (int i = 0; i < NUM_TD; i++) {
		int runtime = tasks[i].user_time_useconds;
		if (runtime == 0) continue;
		kprintf("Task ");
		for (int j = 0; j <= tasks[i].tid / NUM_TD; j++) {
			if (j > 0) kprintf(", ");
			kprintf("%d", tasks[i].tid%NUM_TD + j*NUM_TD);
		}
		kprintf(" ran for %d us" EOL, runtime);
		tasks_runtime_us += runtime;
	}
	kprintf("Kernel ran for %d us" EOL, total_runtime_us - tasks_runtime_us);
	kprintf("Ran for %d us total" EOL, total_runtime_us);
}

static void check_stack_canary(void *stack, int tid) {
	unsigned *canary = (unsigned*) stack;
	for (unsigned i = 0; i < sizeof(stack_canary) / sizeof(stack_canary[0]); i++) {
		KASSERTF(canary[i] == stack_canary[i], "Failed stack canary check for task %d, addr %x (%x != %x)", tid, (unsigned) stack, canary[i], stack_canary[i]);
	}
}

void task_check_stack_canary(struct task_descriptor *td) {
	int tid = td->tid;
	// check the canary at the end of our stack
	check_stack_canary(stacks[tid % NUM_TD], tid);

	// check the canary before the start of our stack (the next task's stack bottom)
	// don't do this check if this is the last task descriptor, since there is no
	// canary above
	if (tid % NUM_TD < NUM_TD - 1) {
		check_stack_canary(stacks[tid % NUM_TD + 1], tid);
	}
}
