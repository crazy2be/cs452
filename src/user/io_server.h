#pragma once

// TODO: rename the functions in io.c to not conflict, and then remove prefix

int iosrv_puts(const int channel, const char *str);
int iosrv_putc(const int channel, const char c);

int iosrv_gets(const int channel, char *buf, int len);
int iosrv_getc(const int channel);

void ioserver(const int priority, const int channel, const char *name);
