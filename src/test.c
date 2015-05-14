#include "util.h"
#include "task_descriptor.h"
#include "io.h"

#include "drivers/timer.h"

/* static struct task_context old_context, new_context; */

/* void context_switch(void); */

/* void test(void) { */
/* } */

// From https://balau82.wordpress.com/2010/02/28/hello-world-for-bare-metal-arm-using-qemu/
volatile unsigned int * const UART0DR = (unsigned int *)0x101f1000;
void print_uart0(const char *s) {
    while(*s != '\0') { /* Loop until end of string */
        *UART0DR = (unsigned int)(*s); /* Transmit char */
        s++; /* Next char */
    }
}

int main (int argc, char *argv[]) {
	print_uart0("Hello world!\n");
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

    /* int test2 = 7; */
    /* void (*fp)(void) = &test; */
    /* bwsetfifo(COM2, 1); */
    /* bwsetspeed(COM2, 115200); */
    /* bwprintf(COM2, "Hello world from main\n\r"); */

    /* // set up pretend link register in other stack */
    /* new_context.stack_pointer = 0x80000; */
    /* *(int*) new_context.stack_pointer = (unsigned) &test; */
    /* context_switch(); */

    /* bwprintf(COM2, "Hello world from main again here\n\r"); */

    /* /1* fp(); *1/ */
    /* return test2; */
}
