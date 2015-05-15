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

int highest_priority(int n) {
	int n2 = n & -n; // MAGIC that makes only lowest bit set in result
	printf("n2: %x ", n2);

	switch (n2) {
	case 0b00000000000000000000000000000001: return 0;
	case 0b00000000000000000000000000000010: return 1;
	case 0b00000000000000000000000000000100: return 2;
	case 0b00000000000000000000000000001000: return 3;
	case 0b00000000000000000000000000010000: return 4;
	case 0b00000000000000000000000000100000: return 5;
	case 0b00000000000000000000000001000000: return 6;
	case 0b00000000000000000000000010000000: return 7;
	case 0b00000000000000000000000100000000: return 8;
	case 0b00000000000000000000001000000000: return 9;
	case 0b00000000000000000000010000000000: return 10;
	case 0b00000000000000000000100000000000: return 11;
	case 0b00000000000000000001000000000000: return 12;
	case 0b00000000000000000010000000000000: return 13;
	case 0b00000000000000000100000000000000: return 14;
	case 0b00000000000000001000000000000000: return 15;
	case 0b00000000000000010000000000000000: return 16;
	case 0b00000000000000100000000000000000: return 17;
	case 0b00000000000001000000000000000000: return 18;
	case 0b00000000000010000000000000000000: return 19;
	case 0b00000000000100000000000000000000: return 20;
	case 0b00000000001000000000000000000000: return 21;
	case 0b00000000010000000000000000000000: return 22;
	case 0b00000000100000000000000000000000: return 23;
	case 0b00000001000000000000000000000000: return 24;
	case 0b00000010000000000000000000000000: return 25;
	case 0b00000100000000000000000000000000: return 26;
	case 0b00001000000000000000000000000000: return 27;
	case 0b00010000000000000000000000000000: return 28;
	case 0b00100000000000000000000000000000: return 29;
	case 0b01000000000000000000000000000000: return 30;
	case 0b10000000000000000000000000000000: return 31;
	}
	return -1;
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

	int n = 0x45;
	printf("n: %x h: %d ", n, highest_priority(n));
	io_flush(COM2);

    test();

    void *sp = init_task((void*) 0x200000, &test2);

    void *sp2 = exit_kernel(sp);

    test();
    return sp == sp2;
}
