#include <kernel.h>
#include "util.h"
#include "task_descriptor.h"
#include "io.h"
#include "drivers/timer.h"
#include "context_switch.h"

#define MAX_TID 255

void test2(void) {
    for (;;) {
        io_puts(COM2, "Inside test function 2\n\r");
        io_flush(COM2);
        pass();
    }
}

static void setup(void) {
	uart_configure(COM1, 2400, OFF);
	uart_configure(COM2, 115200, OFF);

	uart_clrerr(COM1);
	uart_clrerr(COM2);

	timer_init();
	io_init();

	while (!uart_canwrite(COM2)) {}
	uart_write(COM2, 'B');
	while (!uart_canwrite(COM2)) {}
	uart_write(COM2, 'o');
	while (!uart_canwrite(COM2)) {}
	uart_write(COM2, 'o');
	while (!uart_canwrite(COM2)) {}
	uart_write(COM2, 't');
	while (!uart_canwrite(COM2)) {}
	uart_write(COM2, '.');
	while (!uart_canwrite(COM2)) {}
	uart_write(COM2, '.');
	while (!uart_canwrite(COM2)) {}
	uart_write(COM2, '.');

	io_puts(COM2, "IO...\n\r");
	io_flush(COM2);

	printf("e1:%x ", uart_err(COM1));
	printf("e2:%x ", uart_err(COM2));
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
}

struct task_collection {
    // for now, a TID is just an index into this array
    // eventually, this will no longer scale
    struct task_descriptor task_buf[MAX_TID + 1];
    // next tid to allocate
    int next_tid;
    int active_tasks;
};

static struct task_collection tasks;

struct task_descriptor *create_task(void *entrypoint, int priority, int parent_tid) {
    void *sp = (void*) 0x200000;
    struct task_descriptor *task = &tasks.task_buf[tasks.next_tid];

    tasks.active_tasks++;

    task->tid = tasks.next_tid++;
    task->parent_tid = parent_tid;
    task->priority = priority;
    task->state = READY;
    /* task->memory_segment = sp; */

    sp = init_task_stack(sp, entrypoint);

    task->context.stack_pointer = sp;

    return task;
}

int main(int argc, char *argv[]) {
    tasks.next_tid = 0;
    tasks.active_tasks = 0;
    struct task_descriptor * current_task;

    setup();

    // set up the first task

    current_task = create_task(&test2, 0, 0);
    (void) current_task;

	io_puts(COM2, "Starting task scheduling\n\r");
	io_flush(COM2);

    tasks.active_tasks = 10; // hack for testing
    while (tasks.active_tasks > 0) {
        tasks.active_tasks--;
        struct syscall_context sc;
        sc = exit_kernel(current_task->context.stack_pointer);
        current_task->context.stack_pointer = sc.stack_pointer;
    }

	io_puts(COM2, "No more tasks; done task scheduling\n\r");
	io_flush(COM2);

    return 0;
}

