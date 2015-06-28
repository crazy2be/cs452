#pragma once

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
int fgetc(const int channel);

int fprintf(int channel, const char *format, ...);
int snprintf(char *buf, unsigned size, const char *fmt, ...);

// Drop whatever is in the input buffer
// This is useful for discarding garbage input produced by the train controller
// at the start of the run
// A more general solution might be to do a non-blocking gets which only returns stuff
// already in the buffer, and then the client could just discard that.
// However, we don't need this right now, so I don't want to build it.
int fdump(const int channel);
int fbuflen(const int channel);

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

// do all of the initialization needed to start an IO server with the given parameters
// (because we need to pass data in, we need to do some message passing in addition
// to just creating the task)
void ioserver(const int priority, const int channel);
void ioserver_stop(const int channel);
