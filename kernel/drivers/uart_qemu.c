#ifdef QEMU
#include "uart.h"
#include <io.h>
#include "../util.h"

// included here since this defines a bunch of UART flags which are the same
// in the qemu pbversatile arch
#include "ts7200.h"

// https://balau82.wordpress.com/2010/11/30/emulating-arm-pl011-serial-ports/
// http://infocenter.arm.com/help/topic/com.arm.doc.ddi0183f/DDI0183.pdf
#define UART0 0x101F1000
#define UART1 0x101F2000
#define DR 0

static volatile int* reg(int channel, int off) {
	switch (channel) {
		// Intentionally reversed- in ts7200, COM2 is stdout, and COM1 is
		// the train controller, but in QEMU, UART0 is stdout.
	case COM1: return (int*)(UART1 + off);
	case COM2: return (int*)(UART0 + off);
	default: KASSERT(0 && "Invalid channel"); return (int*)-1;
	}
}

int uart_err(int channel) {
	return *reg(channel, UART_RSR_OFFSET); // TODO
}
void uart_clrerr(int channel) {
	*reg(channel, UART_RSR_OFFSET) = 0;
}
void uart_configure(int channel, int speed, int fifo) {
	// TODO: We should actually set the speed, turn off the fifo, etc. here,
	// as that will allow more accurate emulation of the real hardware.
}
int uart_canwrite(int channel) {
	return !(*reg(channel, UART_FLAG_OFFSET) & TXFF_MASK);
}
void uart_write(int channel, char c) {
	*reg(channel, DR) = c;
}
int uart_canread(int channel) {
	return !(*reg(channel, UART_FLAG_OFFSET) & RXFE_MASK);
}
char uart_read(int channel) {
	return *reg(channel, DR);
}
#endif
