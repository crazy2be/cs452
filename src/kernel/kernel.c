#include <kernel.h>

#include <util.h>
#include <least_significant_set_bit.h>
#include <prng.h>

#include "drivers/timer.h"
#include "drivers/uart.h"
#include "drivers/irq.h"
#include "context_switch.h"
#include "tasks.h"
#include "kassert.h"
#include "syscalls/syscalls.h"

/** @file */

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
	uart_configure(COM2, 115200, ON);

	// clear UART errors
	uart_clrerr(COM1);
	uart_clrerr(COM2);

	fputs(COM2_DEBUG, "Boot");

	clear_bss();
	fputc(COM2_DEBUG, '.');

	setup_irq_table();
	fputc(COM2_DEBUG, '.');

	await_init();
	fputc(COM2_DEBUG, '.');

	irq_setup();
	fputc(COM2_DEBUG, '.');

	setup_cache();
	fputc(COM2_DEBUG, '.');

	timer_init();
	fputc(COM2_DEBUG, '.');

	fputs(COM2_DEBUG, "IO..." EOL);

	rand_init(0xdeadbeef);

	tasks_init();
}

static void cleanup(void) {
	// If we don't deinit the timer, the second time our kernel boots up, we
	// end up crashing immediately (or, at least, before printing anything to
	// the screen). Deiniting the timer fixes this, but it seems like it's
	// probably the wrong solution. Our current most likely hypothesis is that
	// this is caused because leaving the timer enabled causes us to have an
	// interrupt pending immediately upon entering the kernel on bootup, and
	// we aren't handling this casse correctly. Disabling the timer when
	// shutting down works around this issue, but doesn't seem like it will
	// work properly once we have other types of interrupts as well (i.e. UART).
	//
	// On pgrabound-experimental, we have some code to disable interrupts again
	// on kernel shutdown, but, curiously, that code doesn't seem to fix this
	// problem. More investigation is needed...
	timer_deinit();
	tick_timer_clear_interrupt();
	uart_cleanup(COM1);
	uart_cleanup(COM2);
	irq_cleanup();
}

void idle_task(void) {
	for (;;) {
		if (!should_idle()) break;
		for (volatile int i = 0; i < 1000; i++) {}
	}
}

#include "../gen/syscalls.h"
#define SYSCALL_IRQ 37
int boot(void (*init_task)(void), int init_task_priority, int debug) {
	setup();
	unsigned ts_start = debug_timer_useconds();

	task_schedule(task_create(idle_task, PRIORITY_IDLE, 0));
	struct task_descriptor *current_task = task_create(init_task, init_task_priority, 0);
	int running = 1;
	do {
		KASSERT(current_task->state == READY);
		struct syscall_context sc;

		unsigned ts_before = debug_timer_useconds();
		// context switch to the next task to be run
		sc = exit_kernel(current_task->context);
		unsigned ts_after = debug_timer_useconds();

		current_task->user_time_useconds += ts_after - ts_before;

		// after returning from that task, save it's place in the task's
		// task descriptor
		current_task->context = sc.context;

		// respond to the syscall that it made
		// after responding to the syscall, schedule it for execution
		// on the appropriate queue (unless exit is called, in which
		// case the task is not rescheduled).
		switch (sc.syscall_num) {
		case SYSCALL_CREATE:      create_handler(current_task);      break;
		case SYSCALL_PASS:        pass_handler(current_task);        break;
		case SYSCALL_EXITK:       exit_handler(current_task);        break;
		case SYSCALL_TID:         tid_handler(current_task);         break;
		case SYSCALL_PARENT_TID:  parent_tid_handler(current_task);  break;
		case SYSCALL_SEND:        send_handler(current_task);        break;
		case SYSCALL_RECEIVE:     receive_handler(current_task);     break;
		case SYSCALL_REPLY:       reply_handler(current_task);       break;
		case SYSCALL_AWAIT:       await_handler(current_task);       break;
		case SYSCALL_RAND:        rand_handler(current_task);        break;
		case SYSCALL_SHOULD_IDLE: should_idle_handler(current_task); break;
		case SYSCALL_IRQ:         irq_handler(current_task);         break;
		case SYSCALL_HALT:        running = 0;                       break;
		default:
			KASSERT(0 && "UNKNOWN SYSCALL NUMBER");
			break;
		}
		current_task = task_next_scheduled();
	} while (running && current_task);

	unsigned ts_end = debug_timer_useconds();
	unsigned total_time_useconds = ts_end - ts_start;

	kprintf("Exiting kernel..." EOL);
	cleanup();

	if (debug) {
		tasks_print_runtime(total_time_useconds);
	}
	return 0;
}

