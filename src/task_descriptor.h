#pragma once

struct task_context {
    unsigned stack_pointer;
};

enum task_state { ACTIVE, READY, ZOMBIE };

struct task_descriptor {
    int tid;
    int parent_tid;
    int priority;
    enum task_state state;

    void *memory_segment;

    struct task_context context;
};
