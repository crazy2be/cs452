#include "await_irq.h"

#include <io.h>

#include "await.h"
#include "../tasks.h"
#include "../kassert.h"
#include "../drivers/uart.h"

struct uart_state {
	unsigned rx_pending : 1;
	unsigned tx_pending : 1;
};

struct uart_state states[2];

void io_irq_init(void) {
	for (int i = COM1; i <= COM2; i++) {
		states[i].rx_pending = 0;
		states[i].tx_pending = 0;
	}
}

static void write_handler(int channel, struct task_descriptor *td) {
	uart_write(channel, (unsigned char) syscall_arg(td->context, 1));
	syscall_set_return(td->context, 0);
	task_schedule(td);
}

static void read_handler(int channel, struct task_descriptor *td) {
	syscall_set_return(td->context, uart_read(channel));
	task_schedule(td);
}

void io_irq_handler(int channel) {
	int irq_mask = uart_irq_type(channel);
	struct uart_state *st = &states[channel];
	int eid;
	struct task_descriptor *td;
	if (UART_IRQ_IS_RX(irq_mask)) {

		eid = EID_COM1_READ + 2 * channel;
		KASSERT(uart_canread(channel));
		KASSERT(!st->rx_pending);
		td = get_awaiting_task(eid);

		if (td) {
			read_handler(channel, td);
		} else {
			// wait until we have a task to consume this data
			uart_ack_rx_irq(channel);
			st->rx_pending = 1;
		}
	} else if (UART_IRQ_IS_TX(irq_mask)) {
		KASSERT(!st->tx_pending);

		eid = EID_COM1_WRITE + 2 * channel;
		td = get_awaiting_task(eid);

		if (td) {
			write_handler(channel, td);
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
		write_handler(channel, td);
		st->tx_pending = 0;
		uart_restore_tx_irq(channel);
		return 1;
	} else {
		return 0;
	}
}

int io_irq_check_for_pending_rx(int channel, struct task_descriptor *td) {
	struct uart_state *st = &states[channel];
	if (st->rx_pending) {
		read_handler(channel, td);
		st->rx_pending = 0;
		uart_restore_rx_irq(channel);
		return 1;
	} else {
		return 0;
	}
}
