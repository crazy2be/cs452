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

// TODO: I think we can dedupe some more of this code from rx/tx handler
static void rx_handler(int channel, char **ppbuf, int *pbuflen) {
	char *buf = *ppbuf;
	int buflen = *pbuflen;
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
	*ppbuf = buf;
	*pbuflen = buflen;
}
static void tx_handler(int channel, char **ppbuf, int *pbuflen) {
	char *buf = *ppbuf;
	int buflen = *pbuflen;
	if (USE_FIFO(channel)) {
		// use the TX FIFO, and don't use CTS
		do {
			uart_write(channel, *buf++);
			buflen--;
		} while(uart_canwritefifo(channel) && buflen > 0);
	} else {
		// don't use the FIFO, and do use CTS
		if (states[channel].cts == CTS_READY) {
			uart_write(channel, *buf++);
			buflen--;
			states[channel].cts = CTS_WAITING_TO_DEASSERT;
			uart_restore_modem_irq(channel);
		} else {
			uart_disable_tx_irq(channel);
		}
	}
	*ppbuf = buf;
	*pbuflen = buflen;
}

// watch for changes to the status register of the UART
// this is used to wait for CTS to deassert, then assert again in order
// to properly output to COM1 / the train controller
static void modem_handler(int channel, char **ppbuf, int *pbuflen) {
	KASSERT(channel == COM1 && "We shouldn't receive modem interrupts for COM2");

	// unlike the other interrupts, we have to explicitly clear the modem interrupt
	uart_clear_modem_irq(channel);

	// transition the CTS state machine
	if (states[channel].cts == CTS_WAITING_TO_DEASSERT && !uart_cts(channel)) {
		states[channel].cts = CTS_WAITING_TO_ASSERT;
	} else if (states[channel].cts == CTS_WAITING_TO_ASSERT && uart_cts(channel)) {
		if (*ppbuf && uart_canwrite(channel)) {
			// if there is IO we can do immediately, do it, then restart CTS state machine
			uart_write(channel, *(*ppbuf)++);
			(*pbuflen)--;
			states[channel].cts = CTS_WAITING_TO_DEASSERT;
			// make sure the tx interrupt is activated again, so we notice when TX -> 1
			uart_restore_tx_irq(channel);
		} else {
			// change state so we are ready to output when next requested to do so
			states[channel].cts = CTS_READY;
			// turn off modem interrupts so we don't infinite loop
			uart_disable_modem_irq(channel);
		}
	}
	// ignore any changes to the modem bits which aren't to the CTS bit
}

static int mask_is_tx(int irq_mask) {
	if (UART_IRQ_IS_TX(irq_mask) || UART_IRQ_IS_MODEM(irq_mask)) return 1;
	else if (UART_IRQ_IS_RX(irq_mask)) return 0;
	else {
		kprintf("irq_mask: %d\r\n", irq_mask);
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
	int is_tx = mask_is_tx(irq_mask);
	KASSERT(is_tx ? uart_canwritefifo(channel) : uart_canreadfifo(channel));

	int eid = eid_for_uart(channel, is_tx);
	struct task_descriptor *td = get_awaiting_task(eid);
	// interrupts should be turned off if there is no waiting task
	// so we shouldn't even get this interrupt if there is no waiting task

	char *buf;
	int buflen;
	if (td) {
		buf = (char*) syscall_arg(td->context, 1);
		buflen = (int) syscall_arg(td->context, 2);
		KASSERT(buflen != 0);
	} else {
		// we switch off TX & RX interrupts when there is no waiting task, so
		// since there isn't a waiting task, this must be a modem interrupt
		KASSERT(UART_IRQ_IS_MODEM(irq_mask));
		// set buf to NULL - modem_handler uses this to infer that there is no
		// waiting task, which is a bit of a hack-job
		// TODO: we should refactor that
		buf = NULL;
		buflen = 0;
	}

	if (UART_IRQ_IS_MODEM(irq_mask)) modem_handler(channel, &buf, &buflen);
	else if (is_tx) tx_handler(channel, &buf, &buflen);
	else rx_handler(channel, &buf, &buflen);

	if (buflen > 0) {
		td->context->r1 = (unsigned) buf;
		td->context->r2 = buflen;
	} else if (td) {
		syscall_set_return(td->context, 0);
		task_schedule(td);
		clear_awaiting_task(eid);
		disable_irq(channel, is_tx);
	}
}
void io_irq_mask_add(int channel, int is_tx) {
	if (is_tx) uart_restore_tx_irq(channel);
	else uart_restore_rx_irq(channel);
}
