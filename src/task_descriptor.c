#include "task_descriptor.h"
#include "util.h"

// TODO: these should take into account the priority of the task

struct task_descriptor *task_queue_pop(struct task_queue *q) {
    struct task_descriptor *d = q->last;
    if (d) {
        assert(q->first != 0, "q->first was null, but not q->last");
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
        assert(!q->first->prev, "q->first->prev was not null");
        q->first->prev = d;
    } else {
        assert(q->last == 0, "q->last was null, but not q->first");
        q->last = d;
    }
    q->first = d;
}
