#pragma once

// TODO: rename the functions in io.c to not conflict, and then remove prefix

// all of these functions are blocking
int iosrv_puts(const int channel, const char *str);
int iosrv_putc(const int channel, const char c);

// gets currently blocks until the *entire* buffer is filled with input
// we'll want to change this for COM2, so that it will return as soon as ENTER
// is hit
int iosrv_gets(const int channel, char *buf, int len);
int iosrv_getc(const int channel);

// do all of the initialization needed to start an IO server with the given parameters
// (because we need to pass data in, we need to do some message passing in addition
// to just creating the task)
void ioserver(const int priority, const int channel);
