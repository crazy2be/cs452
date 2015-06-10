#pragma once

// TODO: rename the functions in io.c to not conflict, and then remove prefix
int iosrv_puts(const int channel, const char *str);
int iosrv_getc(const int channel);
/* int putc(const char c); */
void ioserver(const int priority, const int channel, const char *name);
