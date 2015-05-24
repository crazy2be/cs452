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
	char buf[RBUF_SIZE];
};

/**
 * Intializes the buffer to an empty state
 */
void rbuf_init(struct RBuf *rbuf);

/**
 * Adds a character to the ringbuffer
 * There must be space in the buffer, or the result is undefined.
 */
void rbuf_put(struct RBuf *rbuf, char val);

/**
 * Consumes a character from the ringbuffer
 * It must not be empty, or the result is undefined.
 */
char rbuf_take(struct RBuf *rbuf);
