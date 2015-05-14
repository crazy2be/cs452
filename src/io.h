#pragma once

#include "drivers/uart.h"

void io_init();
void io_putc(int channel, char c);
void io_puts(int channel, const char* s);
void io_putll(int channel, long long n);
void io_flush(int channel); // Blocking. For debugging only!
int io_buf_is_empty(int channel);
int io_buflen(int bufn);

int io_hasc(int channel);
char io_getc(int channel);
void io_service(void);
