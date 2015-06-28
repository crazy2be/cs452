#pragma once

/** @file */
#include <util.h>
#include <assert.h>

/**
 * Main entrypoint into the kernel
 */
int boot(void (*init_task)(void), int init_task_priority, int debug);

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
#define Create try_create
#define create(...) ASSERTOK(try_create(__VA_ARGS__))
int try_create(int priority, void *code);
#define CREATE_INVALID_PRIORITY -1
#define CREATE_INSUFFICIENT_RESOURCES -2

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
void exitk(void) __attribute__((noreturn));

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

#define PRIORITY_MAX 0
#define PRIORITY_MIN 30
#define PRIORITY_IDLE 31
#define PRIORITY_COUNT (PRIORITY_IDLE + 1)
// Process priorities are reversed from what you might expect based on normal
// number systems. Use macros to be less confusing, maybe?
#define HIGHER(priority, amount) ((priority) - (amount))
#define LOWER(priority, amount) ((priority) + (amount))

#define Send try_send
#define send(...) ASSERTOK(try_send(__VA_ARGS__))
int try_send(int tid, const void *msg, int msglen, void *reply, int replylen);
#define SEND_IMPOSSIBLE_TID -1
#define SEND_INVALID_TID -2
#define SEND_INCOMPLETE -3

#define Receive try_receive
#define receive(...) ASSERTOK(try_receive(__VA_ARGS__))
#define recv receive
int try_receive(int *tid, void *msg, int msglen);

#define Reply try_reply
#define reply(...) ASSERTOK(try_reply(__VA_ARGS__))
int try_reply(int tid, const void *reply, int replylen);
#define REPLY_SUCCESSFUL 0
#define REPLY_IMPOSSIBLE_TID -1
#define REPLY_INVALID_TID -2
#define REPLY_UNSOLICITED -3
#define REPLY_TOO_LONG -4

#define EID_TIMER_TICK 0
#define EID_COM1_READ 1
#define EID_COM1_WRITE 2
#define EID_COM2_READ 3
#define EID_COM2_WRITE 4
#define EID_NUM_EVENTS 5
#define AwaitEvent try_await
#define await(...) ASSERTOK(try_await(__VA_ARGS__));
int try_await(unsigned eid, char *buf, unsigned buflen);
#define AWAIT_UNKNOWN_EVENT -1
#define AWAIT_MULTIPLE_WAITERS -2

unsigned rand(void);

int should_idle(void); // Just for the idle task.

void halt(void) __attribute__((noreturn)); // stop the kernel immediately, does not return
