#include "priority.h"

void queue_init(struct priority_queue *q) {
    q->size = 0;
}

int queue_push(struct priority_queue *q, int val, int priority) {
    unsigned i, parent;

    if (q->size >= PRIORITY_QUEUE_SIZE) {
        return 1;
    }

    // start at the first empty spot, and try to insert there
    for (i = q->size; i > 0; i = parent) {
        // compare with the parent - if the parent is of lower priority,
        // put the parent in this spot, and try to insert in the parent's spot

        parent = (i - 1) / 2;
        if (q->buf[parent].priority >= priority) break;
        q->buf[i] = q->buf[parent];
    }

    q->buf[i].priority = priority;
    q->buf[i].val = val;
    q->size++;

    return 0;
}

int queue_pop(struct priority_queue *q, int *val) {
    unsigned i, child;
    struct priority_queue_element e;
    if (q->size == 0) {
        return 1;
    }

    // return the value
    *val = q->buf[0].val;

    // peel off the last value, and insert it at the top of the heap,
    // maintaining heap order
    e = q->buf[--q->size];

    for (i = 0; ; i = child) {
        child = i * 2 + 1;
        if (child >= q->size) break;

        if (child + 1 < q->size &&
                q->buf[child].priority < q->buf[child+1].priority) {
            child++;
        }

        if (q->buf[child].priority <= e.priority) break;
        q->buf[i] = q->buf[child];
    }

    q->buf[i] = e;

    return 0;
}
