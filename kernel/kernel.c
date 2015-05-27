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

void setup_tasks(void) {
    tasks.next_tid = 0;
	extern int stack_top;
    tasks.memory_alloc = (void*) &stack_top + USER_STACK_SIZE;
}

void setup_cache(void) {
    unsigned flags;
    __asm__("mrc p15, 0, %0, c1, c0, 0" : "=r"(flags));
#define FLAG_BITS 0x10c
#if BENCHMARK_CACHE
    flags |= FLAG_BITS;
#else
    flags &= ~FLAG_BITS;
#endif
    __asm__ __volatile__ ("mcr p15, 0, %0, c1, c0, 0" : : "r"(flags));
}

void setup(void) {
    setup_cache();

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
	*task = (struct task_descriptor){
		.tid = tasks.next_tid++,
		.parent_tid = parent_tid,
		.priority = priority,
		.state = READY,
	};

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
    int priority = (int) syscall_arg(uc, 0);
    void *code = (void*) syscall_arg(uc, 1);
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

    syscall_return(uc, result);
}

static inline void tid_handler(struct task_descriptor *current_task) {
    syscall_return(get_task_context(current_task), current_task->tid);
}

static inline void parent_tid_handler(struct task_descriptor *current_task) {
    syscall_return(get_task_context(current_task), current_task->parent_tid);
}

static inline void exit_handler(struct task_descriptor *current_task) {
    // TODO: we need to signal to tasks trying to send/receive to this
    // that the send/receive has failed
    // Should we signal when the task exits after receiving, but before replying?
    // some other task might still reply to the message, according to the spec
    current_task->state = ZOMBIE;
    struct task_descriptor *td;
    while ((td = task_queue_pop(&current_task->waiting_for_replies))) {
        // signal to the sending task that the send failed
        syscall_return(get_task_context(td), SEND_INCOMPLETE);
        td->state = READY;
        priority_task_queue_push(&queue, td);
    }
}

static void dispatch_msg(struct task_descriptor *to, struct task_descriptor *from) {
    struct user_context *to_context = get_task_context(to);
    struct user_context *from_context = get_task_context(from);

    // write tid of sender to pointer provided by receiver
    *(unsigned*)syscall_arg(to_context, 0) = from->tid;

    // copy message into buffer
    // truncate it if it won't fit into the receiving buffer
    memcpy((void*) syscall_arg(to_context, 1), (void*) syscall_arg(from_context, 1),
            MIN((int) syscall_arg(to_context, 2), (int) syscall_arg(from_context, 2)));

    // return sent msg len to the receiver
    syscall_return(to_context, syscall_arg(from_context, 2));

	from->state = REPLY_BLK;
	to->state = READY;
	priority_task_queue_push(&queue, to);
}

static inline void send_handler(struct task_descriptor *current_task) {
    struct user_context *uc = get_task_context(current_task);
	int to_tid = syscall_arg(uc, 0);
    // check if the tid exists & is not us
    if (to_tid < 0 || tasks.next_tid <= to_tid || to_tid == current_task->tid) {
        syscall_return(uc, (to_tid < 0 || MAX_TID <= to_tid) ? SEND_IMPOSSIBLE_TID : SEND_INVALID_TID);
        priority_task_queue_push(&queue, current_task);
        return;
    }
	struct task_descriptor *to_td = &tasks.task_buf[to_tid];

    // check that we're not sending to an exited task
    if (to_td->state == ZOMBIE) {
        syscall_return(uc, SEND_INVALID_TID);
        priority_task_queue_push(&queue, current_task);
        return;
    }

	if (to_td->state == RECV_BLK) {
        // if the task we're sending to is waiting for a reply,
        // immediately exchange the message
		dispatch_msg(to_td, current_task);
	} else {
        // otherwise, queue up on that task, and wait to be processed
		task_queue_push(&to_td->waiting_for_replies, current_task);
		current_task->state = SEND_BLK;
	}
}

static inline void receive_handler(struct task_descriptor *current_task) {
	struct task_descriptor *from_td = task_queue_pop(&current_task->waiting_for_replies);
	if (from_td) {
        dispatch_msg(current_task, from_td);
	} else {
		current_task->state = RECV_BLK;
    }
}

static inline void reply_handler(struct task_descriptor *current_task) {
	struct user_context *recv_context = get_task_context(current_task);
	int send_tid = syscall_arg(recv_context, 0);

    // check if the tid exists & is not us
    if (send_tid < 0 || tasks.next_tid <= send_tid || send_tid == current_task->tid) {
        syscall_return(recv_context,
            (send_tid < 0 || MAX_TID <= send_tid) ? REPLY_IMPOSSIBLE_TID : REPLY_INVALID_TID);
        priority_task_queue_push(&queue, current_task);
        return;
    }

	struct task_descriptor *send_td = &tasks.task_buf[send_tid];

    // check that we sending to a task that expects a reply
    if (send_td->state != REPLY_BLK) {
        syscall_return(recv_context, (send_td->state == ZOMBIE) ? REPLY_INVALID_TID : REPLY_UNSOLICITED);
        priority_task_queue_push(&queue, current_task);
        return;
    }

    struct user_context *send_context = get_task_context(send_td);

    // copy the reply back
    int send_len = syscall_arg(send_context, 4);
    int recv_len = syscall_arg(recv_context, 2);

    if (send_len < recv_len) {
        syscall_return(recv_context, REPLY_TOO_LONG);
        priority_task_queue_push(&queue, current_task);
        return;
    }

    memcpy((void*) syscall_arg(send_context, 3), (void*) syscall_arg(recv_context, 1), recv_len);

    // return the length of the reply to the sender
    syscall_return(send_context, recv_len);
    syscall_return(recv_context, REPLY_SUCCESSFUL);

    // queue both the sending and receiving tasks to execute again
	send_td->state = READY;
	priority_task_queue_push(&queue, send_td);
	priority_task_queue_push(&queue, current_task);
}

#include "../gen/syscalls.h"
int boot(void (*init_task)(void), int init_task_priority) {
    struct task_descriptor * current_task;

    setup();

    // set up the first task
    current_task = create_task(init_task, init_task_priority, 0);
    (void) current_task;

    do {
        KASSERT(current_task->state == READY);
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
        KASSERT(1 && "TEST ASSERT");
        switch (sc.syscall_num) {
        case SYSCALL_PASS:
            // no-op
            priority_task_queue_push(&queue, current_task);
            break;
        case SYSCALL_EXITK:
            // drop the current task off the queue

            exit_handler(current_task);
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
		case SYSCALL_SEND:
			send_handler(current_task);
			break;
		case SYSCALL_RECEIVE:
			receive_handler(current_task);
			break;
		case SYSCALL_REPLY:
			reply_handler(current_task);
			break;
        default:
            KASSERT(0 && "UNKNOWN SYSCALL NUMBER");
            break;
        }

        // find the next task scheduled for execution, if any
        current_task = priority_task_queue_pop(&queue);
    } while (current_task);

    return 0;
}

