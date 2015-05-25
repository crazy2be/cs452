#include <kernel.h>
#include "util.h"
#include "task_descriptor.h"
#include "io.h"
#include "drivers/timer.h"
#include "drivers/uart.h"
#include "context_switch.h"

/** @file */

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

static struct task_collection tasks;
static struct priority_task_queue queue;

/**
 * Internal kernel utility for creating a task, and allocating the necessary resources
 *
 * Allocates space on the task buffer as a side-effect, and increments the count
 * of created tasks.
 * It initializes the task descriptor for the newly created task.
 *
 * It also allocates memory for the tasks stack, and initializes it.
 * After the stack has been initalized, in can be context switched to normally by
 * using `exit_kernel()`, and will start executing at the entrypoint.
 *
 * Notedly, this does *not* schedule the newly created task for execution.
 */
struct task_descriptor *create_task(void *entrypoint, int priority, int parent_tid) {
    void *sp = tasks.memory_alloc;
    tasks.memory_alloc += USER_STACK_SIZE;
    struct task_descriptor *task = &tasks.task_buf[tasks.next_tid];

    task->tid = tasks.next_tid++;
    task->parent_tid = parent_tid;
    task->priority = priority;
    /* task->state = READY; */
    /* task->memory_segment = sp; */

    struct user_context *uc = ((struct user_context*) sp) - 1;
    uc->pc = (unsigned) entrypoint;
    uc->lr = (unsigned) &exitk;
    // leave most everything else uninitialized
    uc->r12 = (unsigned) sp;
    unsigned cpsr;
    __asm__ ("mrs %0, cpsr" : "=r" (cpsr));
    uc->cpsr = cpsr & 0xfffffff0;

    task->context = uc;

    return task;
}

static inline struct user_context* get_task_context(struct task_descriptor* task) {
    return task->context;
}

// Implementation of syscalls
//
// All that any of these functions should do is to retrieve arguments from
// the context of the calling stack, return results to the context,
// and perform validation.
// There should be very little application logic in this code.

static inline void create_handler(struct task_descriptor *current_task) {
    struct user_context *uc = get_task_context(current_task);
    int priority = (int) uc->r0;
    void *code = (void*) uc->r1;
    int result;

    if (priority < PRIORITY_MAX || priority > PRIORITY_MIN) {
        result = CREATE_INVALID_PRIORITY;
    } else if (tasks.next_tid > MAX_TID) {
        result = CREATE_INSUFFICIENT_RESOURCES;
    } else {
        struct task_descriptor *new_task = create_task(code, priority, current_task->tid);
        priority_task_queue_push(&queue, new_task);
        result = new_task->tid;
    }

    uc->r0 = result;
}

static inline void tid_handler(struct task_descriptor *current_task) {
    struct user_context *uc = get_task_context(current_task);
    uc->r0 = current_task->tid;
}

static inline void parent_tid_handler(struct task_descriptor *current_task) {
    struct user_context *uc = get_task_context(current_task);
    uc->r0 = current_task->parent_tid;
}

extern int stack_top;

void setup_tasks(void) {
    tasks.next_tid = 0;
    tasks.memory_alloc = (void*) &stack_top + USER_STACK_SIZE;
}

/**
 * Performs miscellaeneous set up which is required before actually starting
 * the kernel.
 */
void setup(void) {
    // write to the control registers of the UARTs to properly configure them
    // for transmission
	uart_configure(COM1, 2400, OFF);
	uart_configure(COM2, 115200, OFF);

    // clear UART errors
	uart_clrerr(COM1);
	uart_clrerr(COM2);

	while (!uart_canwrite(COM2)) {} uart_write(COM2, 'B');
	while (!uart_canwrite(COM2)) {} uart_write(COM2, 'o');
	while (!uart_canwrite(COM2)) {} uart_write(COM2, 'o');
	while (!uart_canwrite(COM2)) {} uart_write(COM2, 't');

	// Zero BSS, because we should.
	extern char __bss_start__, __bss_end__;
	memset(&__bss_start__, 0, &__bss_end__ - &__bss_start__);
	while (!uart_canwrite(COM2)) {} uart_write(COM2, '.');

	timer_init();
	while (!uart_canwrite(COM2)) {} uart_write(COM2, '.');
	io_init();
	while (!uart_canwrite(COM2)) {} uart_write(COM2, '.');

	io_puts(COM2, "IO..." EOL);
	io_flush(COM2);

	printf("e1:%x" EOL, uart_err(COM1));
	printf("e2:%x" EOL, uart_err(COM2));
	io_flush(COM2);

	// Copy exception vector from where it's linked/loaded to the start of
	// memory, where ARM expects to find it. Assumes all instructions in the
	// exception vector are branch instructions.
	// http://www.ryanday.net/2010/09/08/arm-programming-part-1/
	extern volatile int exception_vector_table_src_begin;
	volatile int* exception_vector_table_src = &exception_vector_table_src_begin;
	volatile int* exception_vector_table_dst = (volatile int*)0x0;
	// Note this is automatically divided by 4 because pointers.
	unsigned exception_vector_branch_adjustment =
		exception_vector_table_src - exception_vector_table_dst;
	for (int i = 0; i < 8; i++) {
		exception_vector_table_dst[i] = exception_vector_table_src[i]
			+ exception_vector_branch_adjustment;
	}

    priority_task_queue_init(&queue);

    setup_tasks();
}


/**
 * Main loop of the kernel.
 *
 * Initializes the kernel, starts the first user task, then schedules tasks
 * and responds to syscalls until there is no work left to do.
 *
 * This takes argv and argc, but doesn't actually use them.
 * When I tried to make this a void function instead, it would no longer
 * run on the TS7200. *shrug*
 */
#include "../gen/syscalls.h"
int boot(void (*init_task)(void), int init_task_priority) {
    struct task_descriptor * current_task;

    setup();

    // set up the first task
    current_task = create_task(init_task, init_task_priority, 0);
    (void) current_task;

    do {
        struct syscall_context sc;

        // context switch to the next task to be run
        sc = exit_kernel(current_task->context);

        // after returning from that task, save it's place in the task's
        // task descriptor
        current_task->context = sc.context;

        // respond to the syscall that it made
        // after responding to the syscall, schedule it for execution
        // on the appropriate queue (unless exit is called, in which
        // case the task is not rescheduled).
        switch (sc.syscall_num) {
        case SYSCALL_PASS:
            // no-op
            priority_task_queue_push(&queue, current_task);
            break;
        case SYSCALL_EXITK:
            // drop the current task off the queue
            break;
        case SYSCALL_TID:
            tid_handler(current_task);
            priority_task_queue_push(&queue, current_task);
            break;
        case SYSCALL_PARENT_TID:
            parent_tid_handler(current_task);
            priority_task_queue_push(&queue, current_task);
            break;
        case SYSCALL_CREATE:
            create_handler(current_task);
            priority_task_queue_push(&queue, current_task);
            break;
        default:
            assert(0, "UNKNOWN SYSCALL NUMBER");
            break;
        }

        // find the next task scheduled for execution, if any
        current_task = priority_task_queue_pop(&queue);
    } while (current_task);

    return 0;
}

