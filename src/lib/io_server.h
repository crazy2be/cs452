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
int fputs(const int channel, const char *str);
int fputc(const int channel, const char c);

// gets currently blocks until the *entire* buffer is filled with input
// we'll want to change this for COM2, so that it will return as soon as ENTER
// is hit
int fgets(const int channel, char *buf, int len);
int fgetc(const int channel);

int fprintf(int channel, const char *format, ...);

// shorthand for all the above
#define puts(...) fputs(COM2, __VA_ARGS__)
#define putc(...) fputc(COM2, __VA_ARGS__)
#define gets(...) fgets(COM2, __VA_ARGS__)
#define getc(...) fgetc(COM2, __VA_ARGS__)
#define printf(...) fprintf(COM2, __VA_ARGS__)

#define kprintf(...) fprintf(COM2_DEBUG, __VA_ARGS__)

// do all of the initialization needed to start an IO server with the given parameters
// (because we need to pass data in, we need to do some message passing in addition
// to just creating the task)
void ioserver(const int priority, const int channel);
void ioserver_stop(const int channel);
