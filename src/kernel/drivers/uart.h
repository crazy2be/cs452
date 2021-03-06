#pragma once

#include <kernel.h>
#include "ts7200.h"

#define ON	1
#define	OFF	0

int uart_err(int channel);
void uart_clrerr(int channel);
void uart_configure(int channel, int speed, int fifo);
int uart_canwrite(int channel);
void uart_write(int channel, char c);
int uart_canread(int channel);
char uart_read(int channel);

// FIFO io
int uart_canreadfifo(int channel);
int uart_canwritefifo(int channel);

// CTS
int uart_cts(int channel);

// interrupt related things
void uart_enable_irq(int channel, unsigned mask);
void uart_disable_irq(int channel, unsigned mask);

// individual irq controls deprecated
void uart_disable_rx_irq(int channel);
void uart_restore_tx_irq(int channel);
void uart_disable_tx_irq(int channel);
void uart_restore_rx_irq(int channel);
void uart_disable_modem_irq(int channel);
void uart_restore_modem_irq(int channel);

void uart_clear_modem_irq(int channel);

int uart_irq_mask(int channel);
void uart_cleanup(int channel); // disable interrupts

#define UART_IRQ_IS_RX(x) ((x) & (RTIS_MASK | RIS_MASK))
#define UART_IRQ_IS_TX(x) ((x) & TIS_MASK)
#define UART_IRQ_IS_MODEM(x) ((x) & MIS_MASK)
