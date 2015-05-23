#pragma once

#include <kernel.h>

#define ON	1
#define	OFF	0

int uart_err(int channel);
void uart_clrerr(int channel);
void uart_configure(int channel, int speed, int fifo);
int uart_canwrite(int channel);
void uart_write(int channel, char c);
int uart_canread(int channel);
char uart_read(int channel);
