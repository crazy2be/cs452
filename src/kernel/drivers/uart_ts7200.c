#ifndef QEMU
#include "uart.h"
#include <io.h>
#include "ts7200.h"
#include <util.h>
#include "../kassert.h"

static int* reg(int channel, int off) {
	switch (channel) {
	case COM1: return (int*)(UART1_BASE + off);
	case COM2: return (int*)(UART2_BASE + off);
	default: KASSERT(0 && "Invalid channel"); return (int*)-1;
	}
}

int uart_err(int channel) {
	return *reg(channel, UART_RSR_OFFSET);
}
void uart_clrerr(int channel) {
	*reg(channel, UART_RSR_OFFSET) = 0;
}

/*
 * The UARTs are initialized by RedBoot to the following state
 * 	115,200 bps
 * 	8 bits
 * 	no parity
 * 	fifos enabled
 */
void uart_configure(int channel, int speed, int fifo) {
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
}
int uart_canwrite(int channel) {
	// http://www.cgl.uwaterloo.ca/~wmcowan/teaching/cs452/s12/pdf/flowControl.pdf
	int *flags = reg(channel, UART_FLAG_OFFSET);
	return *flags & TXFE_MASK; // TODO: CTS_MASK?
}
void uart_write(int channel, char c) {
	/* KASSERT(uart_canwrite(channel) && "can't write!"); */
	if (channel == COM1) {
		KASSERT(uart_cts(channel) && "not cts");
	}
	int *data = reg(channel, UART_DATA_OFFSET);
	*data = c;
}
int uart_canread(int channel) {
	int *flags = reg(channel, UART_FLAG_OFFSET);
	return *flags & RXFF_MASK;
}
char uart_read(int channel) {
	/* KASSERT(uart_canread(channel) && "can't read!"); */
	int *data = reg(channel, UART_DATA_OFFSET);
	return *data;
}

int uart_canreadfifo(int channel) {
	// receive fifo is not empty
	return !(*reg(channel, UART_FLAG_OFFSET) & RXFE_MASK);
}

int uart_canwritefifo(int channel) {
	// transmit fifo is not full
	return !(*reg(channel, UART_FLAG_OFFSET) & TXFF_MASK);
}

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

void uart_print_ctrl(int channel) {
	printf("For channel %d:...\r\n", channel);
	printf(" MSC: %u\r\n", *reg(channel, UART_CTLR_OFFSET));
	printf(" RIS: %u\r\n", *reg(channel, UART_INTR_OFFSET));
	printf(" MIS: %u\r\n", *reg(channel, 0x40));
}

int uart_irq_mask(int channel) {
	return *reg(channel, UART_INTR_OFFSET);
}

void uart_cleanup(int channel) {
	volatile int *ctrl = reg(channel, UART_CTLR_OFFSET);
	*ctrl = UARTEN_MASK;
}
#endif
