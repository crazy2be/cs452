#include <kernel.h>
#include "util.h"
#include "io.h"
#include "drivers/timer.h"
#include "drivers/uart.h"
#include "drivers/irq.h"
#include "context_switch.h"
#include "prng.h"
#include "tasks.h"
#include "msg.h"
#include "await.h"
#include "least_significant_set_bit.h"

/** @file */

static struct prng random_gen;

void setup_cache(void) {
    unsigned flags;
    __asm__("mrc p15, 0, %0, c1, c0, 0" : "=r"(flags));
#define FLAG_BITS 0x1004
#if BENCHMARK_CACHE
    flags |= FLAG_BITS;
#else
    flags &= ~FLAG_BITS;
#endif
    __asm__ __volatile__ ("mcr p15, 0, %0, c1, c0, 0" : : "r"(flags));
}

void setup_irq_table(void) {
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
}

void clear_bss(void) {
	extern char __bss_start__, __bss_end__;
	memset(&__bss_start__, 0, &__bss_end__ - &__bss_start__);
}

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

	clear_bss();
	while (!uart_canwrite(COM2)) {} uart_write(COM2, '.');
    setup_irq_table();
	while (!uart_canwrite(COM2)) {} uart_write(COM2, '.');
    setup_irq();
	while (!uart_canwrite(COM2)) {} uart_write(COM2, '.');
    setup_cache();
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

    prng_init(&random_gen, 0xdeadbeef);

    tasks_init();
}

// Implementation of syscalls
//
// All that any of these functions should do is to retrieve arguments from
// the context of the calling stack, return results to the context,
// and perform validation.
// There should be very little application logic in this code.

static inline void create_handler(struct task_descriptor *current_task) {
    struct user_context *uc = current_task->context;
    int priority = (int) syscall_arg(uc, 0);
    void *code = (void*) syscall_arg(uc, 1);
    int result;

    if (priority < PRIORITY_MAX || priority > PRIORITY_MIN) {
        result = CREATE_INVALID_PRIORITY;
    } else if (tasks_full()) {
        result = CREATE_INSUFFICIENT_RESOURCES;
    } else {
        struct task_descriptor *new_task = task_create(code, priority, current_task->tid);
        task_schedule(new_task);
        result = new_task->tid;
    }

    syscall_set_return(uc, result);
	task_schedule(current_task);
}

static inline void pass_handler(struct task_descriptor *current_task) {
	// no-op, just reschedule.
	task_schedule(current_task);
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
        syscall_set_return(td->context, SEND_INCOMPLETE);
        td->state = READY;
        task_schedule(td);
    }
	// drop the current task off the queue
}

static inline void tid_handler(struct task_descriptor *current_task) {
    syscall_set_return(current_task->context, current_task->tid);
	task_schedule(current_task);
}

static inline void parent_tid_handler(struct task_descriptor *current_task) {
    syscall_set_return(current_task->context, current_task->parent_tid);
	task_schedule(current_task);
}

static inline void rand_handler(struct task_descriptor *current_task) {
    current_task->context->r0 = prng_gen(&random_gen);
    task_schedule(current_task);
}

static inline void irq_handler(struct task_descriptor *current_task) {
    unsigned long long irq_mask = get_irq();
    unsigned irq_mask_lo = irq_mask;
    unsigned irq_mask_hi = irq_mask >> 32;
    unsigned irq;

    if (irq_mask_lo) {
        irq = least_significant_set_bit(irq_mask_lo);
    } else {
        irq = 32 + least_significant_set_bit(irq_mask_hi);
    }

    printf("GOT AN INTERRUPT %d raw = (%x %x)" EOL, irq, irq_mask_hi, irq_mask_lo);

    switch (irq) {
    case IRQ_TIMER:
        timer_clear_interrupt();
		await_event_occurred(EID_TIMER_TICK, irq);
        break;
    default:
        KASSERT(0 && "UNKNOWN INTERRUPT!");
        break;
    }
    task_schedule(current_task);
    printf("GOT AN INTERRUPT %d" EOL, irq);
}

#include "../gen/syscalls.h"
int boot(void (*init_task)(void), int init_task_priority) {
    setup();

    struct task_descriptor *current_task = task_create(init_task, init_task_priority, 0);
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
        switch (sc.syscall_num) {
        case SYSCALL_CREATE:     create_handler(current_task);     break;
        case SYSCALL_PASS:       pass_handler(current_task);       break;
		case SYSCALL_EXITK:      exit_handler(current_task);       break;
        case SYSCALL_TID:        tid_handler(current_task);        break;
        case SYSCALL_PARENT_TID: parent_tid_handler(current_task); break;
		case SYSCALL_SEND:       send_handler(current_task);       break;
		case SYSCALL_RECEIVE:    receive_handler(current_task);    break;
		case SYSCALL_REPLY:      reply_handler(current_task);      break;
		case SYSCALL_AWAIT:      await_handler(current_task);      break;
        case SYSCALL_RAND:       rand_handler(current_task);       break;
        case 37: irq_handler(current_task); break;
        default:
            KASSERT(0 && "UNKNOWN SYSCALL NUMBER");
            break;
        }
//         do {
			current_task = task_next_scheduled();
// 		} while (!current_task && await_tasks_waiting());
    } while (current_task);

	printf("Exiting kernel...\n");
    return 0;
}

