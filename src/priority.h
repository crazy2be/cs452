#pragma once

// neither of these should be directly manipulated by
// callers - they are only here so callers know how to allocate
// space for a priority queue

#define PRIORITY_QUEUE_SIZE 256

struct priority_queue_element {
    int priority;
    int val;
};

struct priority_queue {
    struct priority_queue_element buf[PRIORITY_QUEUE_SIZE];
    unsigned size;
};

void queue_init(struct priority_queue *q);

// push a value into the queue with the given prority
// if there is insufficient space, 1 is returned.
// otherwise, the insertion is completed successfully, and 0 is returned
int queue_push(struct priority_queue *q, int val, int priority);

// if the queue is non-empty, pop the highest priority element out of the queue
// the value is written to the given pointer, and 0 is returned.
// else, 1 is returned
int queue_pop(struct priority_queue *q, int *val);
