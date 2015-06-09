#include "await_irq.h"

#include <io.h>

#include "await.h"
#include "../tasks.h"
#include "../kassert.h"
#include "../drivers/uart.h"

struct uart_state {
	// rx_pending and tx_pending indicate that we received an rx or tx interrupt
	// at some point in the past, but couldn't respond to it then
	unsigned rx_pending : 1;
	unsigned tx_pending : 1;
	unsigned fifo_enabled : 1;
};

struct uart_state states[2];

void io_irq_init(void) {
	for (int i = COM1; i <= COM2; i++) {
		states[i].rx_pending = states[i].tx_pending = 0;
	}
	// TODO: should COM1 have the fifo turned on? can this coexist with CTS?
	states[0].fifo_enabled = 1;
	states[1].fifo_enabled = 1;
}

// a single await can do multiple bytes of IO - these return 1 if the task
// has finished doing IO, 0 otherwise
static int write_handler(int channel, struct task_descriptor *td) {
	char *buf = (char*) syscall_arg(td->context, 1);
	unsigned buflen = syscall_arg(td->context, 2);


	KASSERT(buflen != 0);
	int b = buflen;

	if (states[channel].fifo_enabled) {
		do {
			uart_write(channel, *buf++);
			buflen--;
		} while(uart_canwritefifo(channel) && buflen > 0);
	} else {
		uart_write(channel, *buf++);
		buflen--;
	}
	printf("Wrote %d bytes in one go!" EOL, b - buflen);

	if (buflen == 0) {
		syscall_set_return(td->context, 0);
		task_schedule(td);
		return 1;
	} else {
		// update arguments, and go back to sleep until more IO can be done
		td->context->r1 = (unsigned) buf;
		td->context->r2 = buflen;
		return 0;
	}
}

static int read_handler(int channel, struct task_descriptor *td) {
	char *buf = (char*) syscall_arg(td->context, 1);
	unsigned buflen = syscall_arg(td->context, 2);

	KASSERT(buflen != 0);

	if (states[channel].fifo_enabled) {
		do {
			*buf++ = uart_read(channel);
			buflen--;
		} while(uart_canreadfifo(channel) && buflen > 0);
	} else {
		*buf++ = uart_read(channel);
		buflen--;
	}

	if (buflen == 0) {
		syscall_set_return(td->context, 0);
		task_schedule(td);
		return 1;
	} else {
		td->context->r1 = (unsigned) buf;
		td->context->r2 = buflen;
		return 0;
	}
}

void io_irq_handler(int channel) {
	int irq_mask = uart_irq_type(channel);
	struct uart_state *st = &states[channel];
	int eid;
	struct task_descriptor *td;
	if (UART_IRQ_IS_RX(irq_mask)) {
		KASSERT(uart_canreadfifo(channel));
		KASSERT(!st->rx_pending);

		eid = EID_COM1_READ + 2 * channel;
		td = get_awaiting_task(eid);

		if (td) {
			if (read_handler(channel, td)) {
				clear_awaiting_task(eid);
			}
		} else {
			// wait until we have a task to consume this data
			uart_ack_rx_irq(channel);
			st->rx_pending = 1;
		}
	} else if (UART_IRQ_IS_TX(irq_mask)) {
		KASSERT(!st->tx_pending);
		KASSERT(uart_canwritefifo(channel));

		eid = EID_COM1_WRITE + 2 * channel;
		td = get_awaiting_task(eid);

		if (td) {
			if (write_handler(channel, td)) {
				clear_awaiting_task(eid);
			}
		} else {
			// wait until we have a task to provide data to write
			uart_ack_tx_irq(channel);
			st->tx_pending = 1;
		}
	} else {
		KASSERT(0 && "UNKNOWN UART IRQ");
	}
}

// these two functions check for tx / rx events already pending,
// and the syscalls immediately return if the event has already happened
// returns non-zero iff the event has already happened (in this case, it
// will already have made the return from the syscall, so there is no
// need to put the task on the await queue)
int io_irq_check_for_pending_tx(int channel, struct task_descriptor *td) {
	struct uart_state *st = &states[channel];
	if (st->tx_pending) {
		st->tx_pending = 0;
		uart_restore_tx_irq(channel);
		return write_handler(channel, td);
	} else {
		return 0;
	}
}

int io_irq_check_for_pending_rx(int channel, struct task_descriptor *td) {
	struct uart_state *st = &states[channel];
	if (st->rx_pending) {
		st->rx_pending = 0;
		uart_restore_rx_irq(channel);
		return read_handler(channel, td);
	} else {
		return 0;
	}
}
