#pragma once

struct task_context {
    void *stack_pointer;
};

enum task_state { ACTIVE, READY, ZOMBIE };

#define PRIORITY_MAX 0
#define PRIORITY_MIN 31
#define PRIORITY_COUNT (PRIORITY_MIN + 1)

struct task_descriptor {
    int tid;
    int parent_tid;
    int priority;

    // We don't have any use for this now, but we probably will later
    /* enum task_state state; */
    /* void *memory_segment; */

    struct task_context context;

    struct task_descriptor *next, *prev;
};

struct task_queue {
    struct task_descriptor *first, *last;
};

void task_queue_init(struct task_queue *q);
struct task_descriptor *task_queue_pop(struct task_queue *q);
void task_queue_push(struct task_queue *q, struct task_descriptor *d);

struct priority_task_queue {
    // If there is a task of priority i in the queue, the i'th bit of
    // the mask will be set (counting bits from least to most significant).
    unsigned priority_queue_mask;
    struct task_queue queues[PRIORITY_COUNT];
};

void priority_task_queue_init(struct priority_task_queue *q);
struct task_descriptor *priority_task_queue_pop(struct priority_task_queue *q);
void priority_task_queue_push(struct priority_task_queue *q, struct task_descriptor *d);
