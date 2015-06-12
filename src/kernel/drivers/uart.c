#include "uart.h"
#include <util.h>
#include "../kassert.h"

static int* reg(int channel, int off) {
	switch (channel) {
	case COM1: return (int*)(UART0_BASE + off);
	case COM2: return (int*)(UART1_BASE + off);
	default: KASSERT(0 && "Invalid channel"); return (int*)-1;
	}
}


//
// Configuration
//

void uart_configure(int channel, int speed, int fifo) {
#ifdef QEMU
	// NOTE: This is a bit of a hack, because in QEMU, the interrupt is
	// edge-sensitive rather than level-sensitive. Thus, we write a byte
	// to get things going.
	while(!uart_canwrite(channel));
	uart_write(channel, 0);
#else
	// Speed - must set speed before fifo
	int *high = reg(channel, UART_LCRM_OFFSET);
	int *low = reg(channel, UART_LCRL_OFFSET);
	switch (speed) {
	case 115200: *high = 0x0; *low = 0x3; break;
	// Was: 128. Calculated by (7372800 / (16 * 2400)) - 1
	// BAUDDIV = (FUARTCLK / (16 * Baud rate)) - 1 p.543 of ep93xx
	case 2400: *high = 0x0; *low = 191; break;
	default: KASSERT(0 && "Unsupported baud");
	}

	// Fifo/flags
	int buf = *reg(channel, UART_LCRH_OFFSET);
	buf = fifo ? buf | FEN_MASK : buf & ~FEN_MASK;
	if (channel == COM1) {
		// 2 bits stop, 8 bit data, no parity
		buf = buf | STP2_MASK; buf = buf & ~PEN_MASK;
	}
	*reg(channel, UART_LCRH_OFFSET) = buf;

	// make sure all interrupts are disabled before starting
	// reset all ctrl flags, turn off all interrupts but leave the uart enabled
	*reg(channel, UART_CTLR_OFFSET) = UARTEN_MASK;
#endif
}

void uart_cleanup(int channel) {
	volatile int *ctrl = reg(channel, UART_CTLR_OFFSET);
#ifdef QEMU
	*ctrl = 0;
#else
	*ctrl = UARTEN_MASK;
#endif
}

int uart_err(int channel) {
	return *reg(channel, UART_RSR_OFFSET); // TODO
}

void uart_clrerr(int channel) {
	*reg(channel, UART_RSR_OFFSET) = 0;
}

//
// Read and write helpers
//

int uart_canwrite(int channel) {
#ifdef QEMU
	return uart_canwritefifo(channel);
#else
	// http://www.cgl.uwaterloo.ca/~wmcowan/teaching/cs452/s12/pdf/flowControl.pdf
	int *flags = reg(channel, UART_FLAG_OFFSET);
	return *flags & TXFE_MASK; // TODO: CTS_MASK?
#endif
}
void uart_write(int channel, char c) {
#ifndef QEMU
	if (channel == COM1) {
		KASSERT(uart_cts(channel) && "not cts");
	}
#endif
	*reg(channel, UART_DATA_OFFSET) = c;
}
int uart_canread(int channel) {
#ifdef QEMU
	return uart_canreadfifo(channel);
#else
	return *reg(channel, UART_FLAG_OFFSET) & RXFF_MASK;
#endif
}
char uart_read(int channel) {
	return *reg(channel, UART_DATA_OFFSET);
}

int uart_canreadfifo(int channel) {
	// receive fifo is not empty
	return !(*reg(channel, UART_FLAG_OFFSET) & RXFE_MASK);
}

int uart_canwritefifo(int channel) {
	// transmit fifo is not full
	return !(*reg(channel, UART_FLAG_OFFSET) & TXFF_MASK);
}

int uart_cts(int channel) {
#ifdef QEMU
	return 1;
#else
	return *reg(channel, UART_FLAG_OFFSET) & CTS_MASK;
#endif
}

//
// Helpers to enable and disable particular interrupt sources of the UART.
// All interrupt sources are OR'd into a single interrupt line on the ICU.
//

void uart_disable_rx_irq(int channel) {
	volatile int *ctrl = reg(channel, UART_CTLR_OFFSET);
	*ctrl &= ~(RIEN_MASK | RTIEN_MASK);
}

void uart_restore_rx_irq(int channel) {
	volatile int *ctrl = reg(channel, UART_CTLR_OFFSET);
	*ctrl |= RIEN_MASK | RTIEN_MASK;
}

void uart_disable_tx_irq(int channel) {
	volatile int *ctrl = reg(channel, UART_CTLR_OFFSET);
	*ctrl &= ~TIEN_MASK;
}

void uart_restore_tx_irq(int channel) {
	volatile int *ctrl = reg(channel, UART_CTLR_OFFSET);
	*ctrl |= TIEN_MASK;
}

void uart_disable_modem_irq(int channel) {
#ifndef QEMU
	volatile int *ctrl = reg(channel, UART_CTLR_OFFSET);
	*ctrl &= ~MSIEN_MASK;
#endif
}
void uart_restore_modem_irq(int channel) {
#ifndef QEMU
	volatile int *ctrl = reg(channel, UART_CTLR_OFFSET);
	*ctrl |= MSIEN_MASK;
#endif
}
void uart_clear_modem_irq(int channel) {
#ifndef QEMU
	*reg(channel, UART_INTR_OFFSET) = 0;
#endif
}

int uart_irq_mask(int channel) {
	return *reg(channel, UART_INTR_OFFSET);
}

