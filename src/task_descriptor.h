#pragma once

struct task_context {
    void *stack_pointer;
};

enum task_state { ACTIVE, READY, ZOMBIE };

struct task_descriptor {
    int tid;
    int parent_tid;
    int priority;
    enum task_state state;

    // We don't have any use for this now, but we probably will later
    /* void *memory_segment; */

    struct task_context context;
};
