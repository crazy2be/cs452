#pragma once

/** @file */

#define RBUF_SIZE 4096

/**
 * A FIFO character ring buffer.
 *
 * Used to buffer IO being produced and consumed, so that IO can be
 * buffered until the UARTs are ready to consume it.
 * This should not be interacted with to perform IO - the buffered IO code
 * should be used instead.
 */
struct RBuf {
	int i; /**< index offset */
	int l; /**< length of data */
	int c; /**< capacity of buffer */
	char buf[RBUF_SIZE];
};

/**
 * Intializes the buffer to an empty state
 *
 * @param rbuf: The buffer to initialize
 */
void rbuf_init(struct RBuf *rbuf);

/**
 * Adds a character to the ringbuffer
 *
 * @param rbuf: The buffer to add the character to.
 * There must be space in the buffer, or the result is undefined.
 * @param val: The character to add.
 */
void rbuf_put(struct RBuf *rbuf, char val);

/**
 * Consumes a character from the ringbuffer
 *
 * @param rbuf: The ring buffer to consume a character from.
 * It must not be empty, or the result is undefined.
 * @return The first character in the buffer
 */
char rbuf_take(struct RBuf *rbuf);
