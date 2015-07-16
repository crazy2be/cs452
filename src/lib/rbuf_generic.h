#include "util.h"
/**
 * A FIFO character ring buffer.
 *
 * Used to buffer IO being produced and consumed, so that IO can be
 * buffered until the UARTs are ready to consume it.
 * This should not be interacted with to perform IO - the buffered IO code
 * should be used instead.
 */

#ifndef RBUF_SIZE
#error No buffer size was provided to ring buffer
#endif
#ifndef RBUF_ELE
#error No element type was provided to ring buffer
#endif
#ifndef RBUF_PREFIX
#error No prefix was provided to ring buffer
#endif

#define RBUF_T struct PASTER(RBUF_PREFIX, rbuf)
#define RBUF_INIT PASTER(RBUF_PREFIX, rbuf_init)
#define RBUF_PUT PASTER(RBUF_PREFIX, rbuf_put)
#define RBUF_TAKE PASTER(RBUF_PREFIX, rbuf_take)
#define RBUF_PEEK PASTER(RBUF_PREFIX, rbuf_peek)
#define RBUF_DROP PASTER(RBUF_PREFIX, rbuf_drop)
#define RBUF_EMPTY PASTER(RBUF_PREFIX, rbuf_empty)
#define RBUF_FULL PASTER(RBUF_PREFIX, rbuf_full)
RBUF_T {
	int i; /**< index offset */
	int l; /**< length of data */
	RBUF_ELE buf[RBUF_SIZE];
};

/**
 * Intializes the buffer to an empty state
 */
void RBUF_INIT(RBUF_T *rbuf);

/**
 * Adds a character to the ringbuffer
 * There must be space in the buffer, or the result is undefined.
 */
void RBUF_PUT(RBUF_T *rbuf, RBUF_ELE val);

/**
 * Consumes a character from the ringbuffer
 * It must not be empty, or the result is undefined.
 */
RBUF_ELE RBUF_TAKE(RBUF_T *rbuf);

const RBUF_ELE *RBUF_PEEK(const RBUF_T *rbuf);
int RBUF_EMPTY(const RBUF_T *rbuf);
int RBUF_FULL(const RBUF_T *rbuf);
void RBUF_DROP(RBUF_T *rbuf, unsigned n);
