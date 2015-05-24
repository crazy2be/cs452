#pragma once

#define COM1 0
#define COM2 1

#ifdef QEMU
#define EOL "\n"
#else
#define EOL "\n\r"
#endif

/**
 * @file
 *
 * Utilities for performing non-blocking, buffered output.
 *
 * For the purposes of this file, a channel number and a UART number
 * are the same thing.
 *
 * Note: some functions refer to buffer numbers instead of channel numbers
 * Use `output_buffer_number()` & `input_buffer_number()` to convert between
 * the two.
 */

// helpers to convert channel numbers -> buffer numbers
static inline int output_buffer_number(int channel) {
    return channel;
}
static inline int input_buffer_number(int channel) {
    return channel + 2;
}

/**
 * Initializes statically allocated IO buffers.
 * Must be called before any of the other functions defined here.
 * `timer_init()` must be called before this, since this uses the timer.
 */
void io_init();

/**
 * Pushes the given character onto the output buffer for that channel.
 * The output buffer will be flushed to the UART in FIFO order.
 * Currently, this just blows up, and corrupts the output buffer
 * if the buffer is full.
 */
void io_putc(int channel, char c);

/**
 * Convenience function for adding an entire string to the output buffer.
 * Equivalent to calling io_putc for each character of the string.
 */
void io_puts(int channel, const char* s);

/**
 * Prints the given number in decimal.
 * Currently buggy - it doesn't print 0, and precedes the output with 'n',
 * and follows the output with 'N'.
 *
 * This function should either be fixed, and integrated with the functionality
 * defined in printf.c, or removed altogether.
 */
void io_putll(int channel, long long n);

/**
 * Flushes the entire output buffer.
 *
 * This blocks until the buffer is flushed,
 * and should be used for debugging only.
 */
void io_flush(int channel);

int io_buf_is_empty(int channel);

/**
 * Note: takes a buffer number, not a channel number
 */
int io_buflen(int bufn);

int io_hasc(int channel);

/**
 * Retrieve input from a channel's input buffer in FIFO order.
 *
 * The caller should check if the input buffer is empty before calling this.
 * If this is called on an empty input channel, the result is undefined, and
 * the input channel will be corrupted.
 *
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

int io_printf(int channel, const char *format, ...);
#define printf(...) io_printf(COM2, __VA_ARGS__)
