#pragma once

#include "drivers/uart.h"

/**
 * @file
 *
 * Utilities for performing non-blocking, buffered output.
 *
 * For the purposes of this file, a channel number and a UART number
 * are the same thing.
 */

/**
 * Initializes statically allocated IO buffers.
 * Must be called before any of the other functions defined here.
 * `timer_init()` must be called before this, since this uses the timer.
 */
void io_init();

/**
 * Pushes the given character onto the output buffer for that channel.
 * The output buffer will be flushed to the UART in FIFO order.
 *
 * @param channel: The channel to output the character to
 * @param c: The character to output
 */
void io_putc(int channel, char c);

/**
 * Convenience function for adding an entire string to the output buffer.
 * Equivalent to calling io_putc for each character of the string.
 *
 * @param channel: The channel to output the character to
 * @param s: The string to output
 */
void io_puts(int channel, const char* s);

/**
 * Prints the given number in decimal.
 * Currently buggy - it doesn't print 0, and precedes the output with 'n',
 * and follows the output with 'N'.
 *
 * This function should either be fixed, and integrated with the functionality
 * defined in printf.c, or removed altogether.
 *
 * @param channel: The channel to output to
 * @param n: The number to output
 */
void io_putll(int channel, long long n);

/**
 * Flushes the entire output buffer.
 *
 * This blocks until the buffer is flushed,
 * and should be used for debugging only.
 *
 * @param channel: The channel whose buffer should be flushed.
 */
void io_flush(int channel);

/**
 * Equivalent to `io_buflen(channel) > 0`.
 *
 * @param channel: The channel whose output buffer should be checked.
 * @return Zero if the output buffer is empty, non-zero otherwise.
 */
int io_buf_is_empty(int channel);

/**
 * @param channel: The channel whose output buffer should be checked.
 * @return The number of bytes currently in the output buffer.
 */
int io_buflen(int bufn);

/**
 * @param channel: The channel whose input buffer should be checked.
 * @return Zero if there are no bytes in the input channel, non-zero otherwise.
 */
int io_hasc(int channel);

/**
 * Retrieve input from a channel's input buffer in FIFO order.
 *
 * The caller should check if the input buffer is empty before calling this.
 * If this is called on an empty input channel, the result is undefined, and
 * the input channel will be corrupted.
 *
 * @param channel: The channel whose buffer input should be taken from
 * @return The first byte in the input buffer.
 * Undefined if the input buffer is empty.
 */
char io_getc(int channel);

/**
 * Attempts to flush a byte from each output buffer to the corresponding UART,
 * and read a byte from each UART into the corresponding input buffer.
 *
 * This does not block, and if no reads or writes can be performed when this is
 * called, nothing happens.
 */
void io_service(void);
