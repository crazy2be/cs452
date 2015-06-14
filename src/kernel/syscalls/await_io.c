#include "await_io.h"

#include "await.h"
#include "../tasks.h"
#include "../kassert.h"
#include "../drivers/uart.h"

// used to maintain CTS state machine for transmitting to COM1
// after transmitting, we want to wait for CTS to go to 0 then 1 again
// and for TX to go to 1 again
#define CTS_WAITING_TO_DEASSERT 0
#define CTS_WAITING_TO_ASSERT 1
#define CTS_READY 2
struct uart_state {
	unsigned cts : 2;
};

struct uart_state states[2];

void io_irq_init(void) {
	states[COM1].cts = CTS_READY;
	states[COM2].cts = CTS_READY;
}

#ifdef QEMU
#define USE_FIFO(channel) 1
#else
#define USE_FIFO(channel) (channel == COM2)
#endif

// if ready to transmit, according to CTS state machine
static int transmit_cts_ready(int channel, int irq_mask) {
	if (UART_IRQ_IS_MODEM(irq_mask)) {
		// modem bits changed; including possible changes to CTS
		// unlike the other interrupts, we have to explicitly clear the modem interrupt
		uart_clear_modem_irq(channel);

		// transition the CTS state machine
		if (states[channel].cts == CTS_WAITING_TO_DEASSERT && !uart_cts(channel)) {
			states[channel].cts = CTS_WAITING_TO_ASSERT;
			return 0;
		} else if (states[channel].cts == CTS_WAITING_TO_ASSERT && uart_cts(channel)) {
			states[channel].cts = CTS_READY;
			if (uart_canwrite(channel)) {
				// ready if TX is high
				return 1;
			} else {
				// otherwise, we're waiting on TX, so shut off modem interrupts
				uart_disable_irq(channel, MSIEN_MASK);
				return 0;
			}
		} else {
			// ignore any changes to the modem bits which aren't to the CTS bit
			return 0;
		}
	} else {
		// TX interrupt
		if (states[channel].cts == CTS_READY) {
			return 1;
		} else {
			// turn TX interrupt off in order to wait for CTS
			uart_disable_irq(channel, TIEN_MASK);
			return 0;
		}
	}
}

static int tx_fifo_handler(int channel, int irq_mask, struct task_descriptor *td) {
	// we shouldn't be able to get modem interrupts, or interrupts without a waiting
	// task, when doing IO configured in FIFO mode
	KASSERT(td && UART_IRQ_IS_TX(irq_mask));

	char* buf = (char*) syscall_arg(td->context, 1);
	int buflen = (int) syscall_arg(td->context, 2);

	do {
		uart_write(channel, *buf++);
		buflen--;
	} while(uart_canwritefifo(channel) && buflen > 0);

	if (buflen <= 0) {
		uart_disable_irq(channel, TIEN_MASK);
		return 1;
	}

	td->context->r1 = (unsigned) buf;
	td->context->r2 = (unsigned) buflen;
	return 0;
}

static int tx_cts_handler(int channel, int irq_mask, struct task_descriptor *td) {
	if (transmit_cts_ready(channel, irq_mask)) {
		if (td) {
			// if there is output ready
			char *buf = (char*) syscall_arg(td->context, 1);
			int buflen = (int) syscall_arg(td->context, 2);

			KASSERT(uart_canwrite(channel) && states[channel].cts == CTS_READY);
			// if there is IO we can do immediately, do it, then restart CTS state machine
			uart_write(channel, *buf++);
			buflen--;

			states[channel].cts = CTS_WAITING_TO_DEASSERT;

			// should leave with just MIS interrupts enabled if no more output,
			// or TX & MIS enabled if there is
			uart_enable_irq(channel, TIEN_MASK | MSIEN_MASK);

			if (buflen <= 0) {
				uart_disable_irq(channel, TIEN_MASK);
				return 1;
			}

			td->context->r1 = (unsigned) buf;
			td->context->r2 = (unsigned) buflen;
			return 0;
		} else {
			// change state so we are ready to output when next requested to do so
			states[channel].cts = CTS_READY;
			// turn off interrupts so we don't infinite loop
			uart_disable_irq(channel, TIEN_MASK | MSIEN_MASK);
			return 0;
		}
	} else {
		// CTS / non-fifo isn't ready
		// any necessary changes to interrupts were done in transmit_cts_ready
		return 0;
	}
}

// td is possibly null (meaning nothing to output, so we'd just transition the output
// state machine).
// return 1 if the task waiting for the output is done, and should exit (should always
// return 0 if the task is null).
static int tx_handler(int channel, int irq_mask, struct task_descriptor *td) {
	if (USE_FIFO(channel)) {
		return tx_fifo_handler(channel, irq_mask, td);
	} else {
		return tx_cts_handler(channel, irq_mask, td);
	}
}

static int rx_handler(int channel, struct task_descriptor *td) {
	char *buf = (char*) syscall_arg(td->context, 1);
	int buflen = (int) syscall_arg(td->context, 2);

	if (USE_FIFO(channel)) {
		// use the RX FIFO
		do {
			*buf++ = uart_read(channel);
			buflen--;
		} while(uart_canreadfifo(channel) && buflen > 0);
	} else {
		// don't use the FIFO, read bytes in 1 at a time
		*buf++ = uart_read(channel);
		buflen--;
	}

	if (buflen <= 0) {
		uart_disable_rx_irq(channel);
		return 1;
	}

	td->context->r1 = (unsigned) buf;
	td->context->r2 = buflen;
	return 0;
}

static int mask_is_tx(int irq_mask) {
	if (UART_IRQ_IS_TX(irq_mask) || UART_IRQ_IS_MODEM(irq_mask)) return 1;
	else if (UART_IRQ_IS_RX(irq_mask)) return 0;
	else {
		kprintf("irq_mask: %d\r\n", irq_mask);
		KASSERT(0 && "UNKNOWN UART IRQ");
		return -1; // unreachable
	}
}
void io_irq_handler(int channel) {
	int irq_mask = uart_irq_mask(channel);
	int is_tx = mask_is_tx(irq_mask);
	KASSERT(is_tx ? uart_canwritefifo(channel) : uart_canreadfifo(channel));

	int eid = eid_for_uart(channel, is_tx);
	struct task_descriptor *td = get_awaiting_task(eid);

	int done = is_tx ? tx_handler(channel, irq_mask, td) : rx_handler(channel, td);

	if (done){
		syscall_set_return(td->context, 0);
		task_schedule(td);
		clear_awaiting_task(eid);
	}
}
void io_irq_mask_add(int channel, int is_tx) {
	if (is_tx) uart_restore_tx_irq(channel);
	else uart_restore_rx_irq(channel);
}
