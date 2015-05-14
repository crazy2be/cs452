#include "util.h"
#include "task_descriptor.h"
#include "io.h"

#include "drivers/timer.h"

/* static struct task_context old_context, new_context; */

/* void context_switch(void); */

void test(void) {
	io_puts(COM2, "Inside test function\n\r");
	io_flush(COM2);
}

int main (int argc, char *argv[]) {
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

    test();
}
