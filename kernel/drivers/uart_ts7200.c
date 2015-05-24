#ifndef QEMU
#include "uart.h"
#include <io.h>
#include "ts7200.h"

extern void assert(int, const char*);

static int* reg(int channel, int off) {
	switch (channel) {
		case COM1: return (int*)(UART1_BASE + off);
		case COM2: return (int*)(UART2_BASE + off);
		default: assert(0, "Invalid channel"); return (int*)-1;
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
		default: assert(0, "Unsupported baud");
	}

	// Fifo/flags
	int buf = *reg(channel, UART_LCRH_OFFSET);
	buf = fifo ? buf | FEN_MASK : buf & ~FEN_MASK;
	if (channel == COM1) {
		// 2 bits stop, 8 bit data, no parity
		buf = buf | STP2_MASK; buf = buf & ~PEN_MASK;
	}
	*reg(channel, UART_LCRH_OFFSET) = buf;
}
int uart_canwrite(int channel) {
	// http://www.cgl.uwaterloo.ca/~wmcowan/teaching/cs452/s12/pdf/flowControl.pdf
	int *flags = reg(channel, UART_FLAG_OFFSET);
	return *flags & TXFE_MASK; // TODO: CTS_MASK?
}
void uart_write(int channel, char c) {
	assert(uart_canwrite(channel), "can't write!");
	if (channel == COM1) {
		assert(*reg(COM1, UART_FLAG_OFFSET) & CTS_MASK, "not cts");
	}
	int *data = reg(channel, UART_DATA_OFFSET);
	*data = c;
}
int uart_canread(int channel) {
	int *flags = reg(channel, UART_FLAG_OFFSET);
	return *flags & RXFF_MASK;
}
char uart_read(int channel) {
	assert(uart_canread(channel), "can't read!");
	int *data = reg(channel, UART_DATA_OFFSET);
	return *data;
}
// void bwputc(int channel, char c) {
// 	while (!(*flags & TXFE_MASK));
// 	// ????
// 	while ((*flags & CTS_MASK));
// 	//while(!(*flags & CTS_MASK));
// }
#endif
