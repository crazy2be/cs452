#include "task_descriptor.h"
#include "util.h"
#include "least_significant_set_bit.h"

void task_queue_init(struct task_queue *q) {
    q->first = q->last = 0;
}

struct task_descriptor *task_queue_pop(struct task_queue *q) {
    struct task_descriptor *d = q->last;
    if (d) {
        KASSERT(q->first != 0 && "q->first was null, but not q->last");
        // if the element existed, remove it from the queue
        q->last = d->prev;
        if (q->last) {
            q->last->next = 0;
        } else {
            q->first = 0;
        }
    }
    return d;
}

void task_queue_push(struct task_queue *q, struct task_descriptor *d) {
    d->next = q->first;
    d->prev = 0;

    if (q->first) {
        KASSERT(!q->first->prev && "q->first->prev was not null");
        q->first->prev = d;
    } else {
        KASSERT(q->last == 0 && "q->last was null, but not q->first");
        q->last = d;
    }
    q->first = d;
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
        if (!pri_queue->first) {
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

    if (!pri_queue->first) {
        // need to set the bit on the mask from zero to one
        q->priority_queue_mask |= 0x1 << priority;
    }

    task_queue_push(pri_queue, d);
}
