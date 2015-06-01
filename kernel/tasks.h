#include "task_descriptor.h"

#define MAX_TID 255
// TODO: eventually, we will want to have variable stack sizes, controlled by some
// parameter to create
#define USER_STACK_SIZE 0x10000 // 64K stack

struct task_collection {
    // for now, a TID is just an index into this array
    // eventually, this will no longer scale
    struct task_descriptor task_buf[MAX_TID + 1];
    // next tid to allocate
    int next_tid;
    void *memory_alloc;
};

void tasks_init(void);

int tasks_full(); // Space for more tasks?

// This does NOT schedule the newly created task for execution.
struct task_descriptor *task_create(void *entrypoint, int priority, int parent_tid);
struct task_descriptor *task_from_tid(int tid);

// Schedule a task for potential future execution.
void task_schedule(struct task_descriptor *task);

struct task_descriptor *task_next_scheduled();

int tid_valid(int tid); // Does TID refer to a living task?
int tid_possible(int tid);
