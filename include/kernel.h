#pragma once

/** @file */

#define PRIORITY_MAX 0
#define PRIORITY_MIN 31
#define PRIORITY_COUNT (PRIORITY_MIN + 1)

/**
 * Make a new task with the given priority and code.
 * @param priority: The highest priority is zero, and decreases as the
 * priority number increases.
 * @param code: Assumed to be a function pointer like: void (*f)(void).
 * This argument is not typed as an actual function pointer because the spec says
 * to do it this way.
 * @return The TID of the newly created task, CREATE_INVALID_PRIORITY if the priority
 * is invalid, or CREATE_INSUFFICIENT_RESOURCES if there are no more task descriptors
 * to be allocated.
 */
#define Create create
int create(int priority, void *code);

#define CREATE_INVALID_PRIORITY -1
#define CREATE_INSUFFICIENT_RESOURCES -2

/**
 * @return The task id of the calling task
 */
#define MyTid tid
int tid(void);

/**
 * @return The task id of the parent of the calling task.
 * This is implementation dependant if the parent has exited.
 * In our implementation, the return is unchanged when the parent exits,
 * and points to a process which is a zombie.
 * Because TIDs are never reused, this can't cause confusion even if
 * the task resources are reused.
 */
#define MyParentTid parent_tid
int parent_tid(void);

/**
 * Yield control flow to the kernel or other tasks.
 * This is a no-op, as far as the caller is concerned.
 */
#define Pass pass
void pass(void);

/**
 * Stop execution of the calling task, and destroy it and all its
 * resources.
 */
#define Exit exitk
void exitk(void);

/**
 * Main entrypoint into the kernel
 */
int boot(void (*init_task)(void), int init_task_priority);
