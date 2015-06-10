#pragma once

// TODO: rename the functions in io.c to not conflict, and then remove prefix
int iosrv_puts(const char *str);
/* int putc(const char c); */
/* int getc(); */
void ioserver(const int priority, const int channel, const char *name);
