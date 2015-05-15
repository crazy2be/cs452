#include "util.h"
#include "task_descriptor.h"
#include "io.h"

#include "drivers/timer.h"
#include "context_switch.h"
#include <kernel.h>

/* static struct task_context old_context, new_context; */


void test(void) {
	io_puts(COM2, "Inside test function\n\r");
	io_flush(COM2);
}

void test2(void) {
	io_puts(COM2, "Inside test function 2\n\r");
	io_flush(COM2);
    pass();
}

int main(int argc, char *argv[]) {
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

    test();

    // set up kernel entrypoint
    /* *(unsigned*) 0x8 = (unsigned) &enter_kernel; */

    void *sp = init_task((void*) 0x200000, &test2);

    void *sp2 = exit_kernel(sp);

    test();
    return sp == sp2;
}
