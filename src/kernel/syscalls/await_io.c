#include "await_io.h"

#include <io.h>

#include "await.h"
#include "../tasks.h"
#include "../kassert.h"
#include "../drivers/uart.h"

struct uart_state {
	unsigned fifo_enabled : 1;
};

struct uart_state states[2];

void io_irq_init(void) {
	// TODO: should COM1 have the fifo turned on? can this coexist with CTS?
	states[0].fifo_enabled = 1;
	states[1].fifo_enabled = 1;
}
// TODO: I think we can dedupe some more of this code from rx/tx handler
static void rx_handler(int channel, char **ppbuf, int *pbuflen, int fifo_enabled) {
	char *buf = *ppbuf;
	int buflen = *pbuflen;
	if (fifo_enabled) {
		do {
			*buf++ = uart_read(channel);
			buflen--;
		} while(uart_canreadfifo(channel) && buflen > 0);
	} else {
		*buf++ = uart_read(channel);
		buflen--;
	}
	*ppbuf = buf;
	*pbuflen = buflen;
}
static void tx_handler(int channel, char **ppbuf, int *pbuflen, int fifo_enabled) {
	char *buf = *ppbuf;
	int buflen = *pbuflen;
	if (fifo_enabled) {
		do {
			uart_write(channel, *buf++);
			buflen--;
		} while(uart_canwritefifo(channel) && buflen > 0);
	} else {
		uart_write(channel, *buf++);
		buflen--;
	}
	*ppbuf = buf;
	*pbuflen = buflen;
}
static int mask_is_tx(int irq_mask) {
	if (UART_IRQ_IS_TX(irq_mask)) return 1;
	else if (UART_IRQ_IS_RX(irq_mask)) return 0;
	else {
		printf("irq_mask: %d\r\n", irq_mask);
		KASSERT(0 && "UNKNOWN UART IRQ");
		return -1;
	}
}
static void disable_irq(int channel, int is_tx) {
	if (is_tx) uart_disable_tx_irq(channel);
	else uart_disable_rx_irq(channel);
}
void io_irq_handler(int channel) {
	int irq_mask = uart_irq_mask(channel);
	printf("Got io_irq with mask %u\n\r", irq_mask);
	int is_tx = mask_is_tx(irq_mask);
	KASSERT(is_tx ? uart_canwritefifo(channel) : uart_canreadfifo(channel));

	int eid = eid_for_uart(channel, is_tx);
	struct task_descriptor *td = get_awaiting_task(eid);
	// interrupts should be turned off if there is no waiting task
	// so we shouldn't even get this interrupt if there is no waiting task
	KASSERT(td);

	char *buf = (char*) syscall_arg(td->context, 1);
	int buflen = (int) syscall_arg(td->context, 2);
	KASSERT(buflen != 0);

	printf("Before: buf: %x, len: %d\r\n", buf, buflen);
	if (is_tx) tx_handler(channel, &buf, &buflen, states[channel].fifo_enabled);
	else rx_handler(channel, &buf, &buflen, states[channel].fifo_enabled);
	printf("After: buf: %x, len: %d\r\n", buf, buflen);

	if (buflen > 0) {
		td->context->r1 = (unsigned) buf;
		td->context->r2 = buflen;
		return;
	}

	syscall_set_return(td->context, 0);
	task_schedule(td);
	clear_awaiting_task(eid);
	disable_irq(channel, is_tx);
}
void io_irq_mask_add(int channel, int is_tx) {
	if (is_tx) uart_restore_tx_irq(channel);
	else uart_restore_rx_irq(channel);
}
