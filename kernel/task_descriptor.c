#include "task_descriptor.h"
#include "util.h"
#include "least_significant_set_bit.h"

void task_queue_init(struct task_queue *q) {
	q->tail = q->head = 0;
}

static inline int task_queue_empty(struct task_queue *q) {
	return !q->head;
}

struct task_descriptor *task_queue_pop(struct task_queue *q) {
	struct task_descriptor *d = q->head;
	if (d) {
		KASSERT(q->tail != 0 && "q->tail was null, but not q->head");
		// if the element existed, remove it from the queue
		q->head = d->queue_next;
		if (!q->head) {
			q->tail = 0;
		}
	}
	return d;
}

void task_queue_push(struct task_queue *q, struct task_descriptor *d) {
	d->queue_next = 0;

	if (q->tail) {
		KASSERT(!q->tail->queue_next && "q->tail->queue_next was not null");
		q->tail->queue_next = d;
	} else {
		KASSERT(q->head == 0 && "q->head was null, but not q->tail");
		q->head = d;
	}
	q->tail = d;
}

void priority_task_queue_init(struct priority_task_queue *q) {
	unsigned i;
	q->priority_queue_mask = 0;
	for (i = 0; i < PRIORITY_COUNT; i++) {
		task_queue_init(&q->queues[i]);
	}
}

struct task_descriptor *priority_task_queue_pop(struct priority_task_queue *q) {
	if (q->priority_queue_mask) {
		int priority = least_significant_set_bit(q->priority_queue_mask);
		struct task_queue *pri_queue = &q->queues[priority];
		struct task_descriptor *d = task_queue_pop(pri_queue);
		if (task_queue_empty(pri_queue)) {
			q->priority_queue_mask &= ~(0x1 << priority);
		}
		return d;
	} else {
		return 0;
	}
}

void priority_task_queue_push(struct priority_task_queue *q, struct task_descriptor *d) {
	int priority = d->priority;
	struct task_queue *pri_queue = &q->queues[priority];

	if (task_queue_empty(pri_queue)) {
		// need to set the bit on the mask from zero to one
		q->priority_queue_mask |= 0x1 << priority;
	}

	task_queue_push(pri_queue, d);
}
