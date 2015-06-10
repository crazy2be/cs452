#ifdef QEMU
#include "uart.h"
#include <io.h>
#include <util.h>
#include "../kassert.h"

// included here since this defines a bunch of UART flags which are the same
// in the qemu pbversatile arch
#include "ts7200.h"

// https://balau82.wordpress.com/2010/11/30/emulating-arm-pl011-serial-ports/
// http://infocenter.arm.com/help/topic/com.arm.doc.ddi0183f/DDI0183.pdf
#define UART0 0x101F1000
#define UART1 0x101F2000
#define UART2 0x101F3000
#define DR 0

// TODO: these should be declared in the same place as the constants from ts7200.h
#undef UART_CTLR_OFFSET
#define UART_CTLR_OFFSET 0x38
#undef UART_INTR_OFFSET
#define UART_INTR_OFFSET 0x40

static volatile int* reg(int channel, int off) {
	switch (channel) {
	case COM1: return (int*)(UART0 + off);
	case COM2: return (int*)(UART1 + off);
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
	if (channel == 0) {
		// NOTE: This is a bit of a hack, because in QEMU, the interrupt is
		// edge-sensitive rather than level-sensitive. Thus, we write a byte
		// to get things going.
		*reg(channel, DR) = 0x00;
	}
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

// COMMON
int uart_canreadfifo(int channel) {
	// receive fifo is not empty
	return !(*reg(channel, UART_FLAG_OFFSET) & RXFE_MASK);
}

// COMMON
int uart_canwritefifo(int channel) {
	// transmit fifo is not full
	return !(*reg(channel, UART_FLAG_OFFSET) & TXFF_MASK);
}

// COMMON
void uart_disable_rx_irq(int channel) {
	volatile int *ctrl = reg(channel, UART_CTLR_OFFSET);
	*ctrl &= ~(RIEN_MASK | RTIEN_MASK);
}

// COMMON
void uart_restore_rx_irq(int channel) {
	volatile int *ctrl = reg(channel, UART_CTLR_OFFSET);
	*ctrl |= RIEN_MASK | RTIEN_MASK;
}

// COMMON
void uart_disable_tx_irq(int channel) {
	printf("Disabling tx irq for channel %d", channel);
	volatile int *ctrl = reg(channel, UART_CTLR_OFFSET);
	*ctrl &= ~TIEN_MASK;
}

// COMMON
void uart_restore_tx_irq(int channel) {
	printf("Restoring tx irq for channel %d", channel);
	volatile int *ctrl = reg(channel, UART_CTLR_OFFSET);
	*ctrl |= TIEN_MASK;
}

void uart_print_ctrl(int channel) {
	printf("For channel %d:...\r\n", channel);
	printf(" MSC: 0x%x\r\n", *reg(channel, UART_CTLR_OFFSET)); // Masks
	printf(" RIS: 0x%x\r\n", *reg(channel, 0x3C)); // Raw interrupt status
	printf(" MIS: 0x%x\r\n", *reg(channel, 0x40)); // Masked interrupt status
}

// COMMON
int uart_irq_type(int channel) {
	return *reg(channel, UART_INTR_OFFSET);
}

void uart_cleanup(int channel) {
	volatile int *ctrl = reg(channel, UART_CTLR_OFFSET);
	*ctrl = 0;
}
#endif
