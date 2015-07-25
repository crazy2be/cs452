#pragma once

#include "vargs.h"

#define COM1 0
#define COM2 1
#define COM2_DEBUG 2 // kernel debug channel, uses BWIO

#ifdef QEMU
#define EOL "\n\r" // This is the same now that we use telnet.
#else
#define EOL "\n\r"
#endif

// all of these functions are blocking
void fput_buf(const char *buf, int buflen, const int channel);
void fputs(const char *str, const int channel);
void fputc(const char c, const int channel);

// gets currently blocks until the *entire* buffer is filled with input
// we'll want to change this for COM2, so that it will return as soon as ENTER
// is hit
void fgets(char *buf, int len, const int channel);
// non-blocking fgets - only return the input currently in the buffer
int fgetsnb(char *buf, int len, const int channel);
int fgetc(const int channel);

int fprintf(int channel, const char *format, ...);
int vsnprintf(char *buf, unsigned size, const char *fmt, va_list va);
int snprintf(char *buf, unsigned size, const char *fmt, ...);

// shorthand for all the above
#define puts(str) fputs(str, COM2)
#define putc(c) fputc(c, COM2)
#define gets(buf, len) fgets(buf, len, COM2)
#define getc() fgetc(COM2)
#define printf(...) fprintf(COM2, __VA_ARGS__)

#define kputs(str) fputs(str, COM2_DEBUG)
#define kputc(c) fputc(c, COM2_DEBUG)
#define kgets(buf, len) fgets(buf, len, COM2_DEBUG)
#define kgetc() fgetc(COM2_DEBUG)
#define kprintf(...) fprintf(COM2_DEBUG, __VA_ARGS__)
