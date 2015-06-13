#pragma once

#define COM1 0
#define COM2 1
#define COM3 2 // kernel debug channel, uses BWIO

#ifdef QEMU
#define EOL "\n\r" // This is the same now that we use telnet.
#else
#define EOL "\n\r"
#endif

// all of these functions are blocking
int puts(const int channel, const char *str);
int putc(const int channel, const char c);

// gets currently blocks until the *entire* buffer is filled with input
// we'll want to change this for COM2, so that it will return as soon as ENTER
// is hit
int gets(const int channel, char *buf, int len);
int getc(const int channel);

int printf(int channel, const char *format, ...);
#define kprintf(...) printf(COM3, __VA_ARGS__)

// do all of the initialization needed to start an IO server with the given parameters
// (because we need to pass data in, we need to do some message passing in addition
// to just creating the task)
void ioserver(const int priority, const int channel);
void ioserver_stop(const int channel);
