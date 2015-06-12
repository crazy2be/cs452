#pragma once

/** @file */

#include <kernel.h>
#include "context_switch.h"

/**
 * FIFO queue of task descriptors implemented as a singly-linked list.
 *
 * The task descriptors themselves contain the prev/next pointers, which
 * allows us to not allocate memory when inserting tasks into the list.
 *
 * Also, implementing the queue as a linked list allows us to have memory
 * usage bounded above by the maximum number of processes, instead of the
 * maximum number of processes times the number of queues.
 * This memory usage characteristic is why the queue is not implemented as
 * a ringbuffer.
 *
 * Insertion and deletion from the queue is constant time.
 */
struct task_descriptor;

// we enqueue onto the tail of the list, and dequeue from the head
struct task_queue {
	struct task_descriptor *tail, *head;
};

/**
 * Initialize the task queue to an empty state
 */
void task_queue_init(struct task_queue *q);

/**
 * Remove the last task descriptor in the queue (remember, the queue is FIFO)
 * and return a pointer to it.
 * If the queue is empty, NULL is returned.
 */
struct task_descriptor *task_queue_pop(struct task_queue *q);

/**
 * Add a task descriptor at the start of the queue.
 */
void task_queue_push(struct task_queue *q, struct task_descriptor *d);


enum task_state { DEAD, READY, SEND_BLK, RECV_BLK, REPLY_BLK };
struct task_descriptor {
	int tid;
	int parent_tid;
	int priority;

	enum task_state state;
	struct task_queue waiting_for_replies;

	// We don't have any use for this now, but we probably will later
	/* void *memory_segment; */

	struct user_context *context;

	struct task_descriptor *queue_next;

	// amount of time in useconds spent in this task (includes time for context
	// switching)
	unsigned user_time_useconds;
};

/**
 * Conceptually like the `task_queue`, but tasks are popped from the queue
 * in priority order.
 *
 * The priority queue is implemented as many separate queues, one for each priority
 * level.
 * This allows maintaining a constant time cost for insertions and deletions in
 * the queue.
 */
struct priority_task_queue {
	/**
	 * A bit mask to make it very cheap to find the priority of the next
	 * task to be popped out.
	 * If there is a task of priority i in the queue, the i'th bit of
	 * the mask will be set (counting bits from least to most significant).
	 * This just caches state about whether each sub-queue is empty or not,
	 * and is purely an optimization.
	 */
	unsigned priority_queue_mask;

	/**
	 * Each priority level has a sub-queue within the priority queue, indexed
	 * by the priority number.
	 */
	struct task_queue queues[PRIORITY_COUNT];
};

/**
 * Conceptually equivalent to task_queue_init
 */
void priority_task_queue_init(struct priority_task_queue *q);

/**
 * Conceptually equivalent to task_queue_pop, except priority is taken into account
 */

struct task_descriptor *priority_task_queue_pop(struct priority_task_queue *q);
/**
 * Conceptually equivalent to task_queue_push
 */
void priority_task_queue_push(struct priority_task_queue *q, struct task_descriptor *d);
