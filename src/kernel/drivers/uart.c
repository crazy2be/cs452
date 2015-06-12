#include "uart.h"

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

int uart_cts(int channel) {
#ifdef QEMU
	return 1;
#else
	return *reg(channel, UART_FLAG_OFFSET) & CTS_MASK;
#endif
}

