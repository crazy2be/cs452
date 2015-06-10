#pragma once
#include "util.h"

// TODO: it's a code smell that data structures asserting in a user task can cause
// the kernel to panic
#include "../kernel/kassert.h"

#define assert_consistent(rbuf) \
	KASSERT(rbuf->l >= 0 && rbuf->l < RBUF_SIZE); \
	KASSERT(rbuf->i >= 0 && rbuf->i < RBUF_SIZE);

void RBUF_INIT(RBUF_T *rbuf) {
	rbuf->i = 0;
	rbuf->l = 0;
	/* for (int i = 0; i < RBUF_SIZE; i++) rbuf->buf[i] = 0; */
}

void RBUF_PUT(RBUF_T *rbuf, RBUF_ELE val) {
	assert_consistent(rbuf);

	KASSERT(rbuf->i < RBUF_SIZE);
	KASSERT(rbuf->l + 1 <= RBUF_SIZE);
	rbuf->buf[(rbuf->i + rbuf->l) % RBUF_SIZE] = val;
	rbuf->l++;
}

RBUF_ELE RBUF_TAKE(RBUF_T *rbuf) {
	assert_consistent(rbuf);

	RBUF_ELE val = rbuf->buf[rbuf->i];
	rbuf->i = (rbuf->i + 1) % RBUF_SIZE;
	rbuf->l--;
	return val;
}

void RBUF_DROP(RBUF_T *rbuf, unsigned n) {
	assert_consistent(rbuf);
	KASSERT(rbuf->l >= n);

	rbuf->i = (rbuf->i + n) % RBUF_SIZE;
	rbuf->l -= n;
}

const RBUF_ELE *RBUF_PEEK(const RBUF_T *rbuf) {
	assert_consistent(rbuf);
	return &rbuf->buf[rbuf->i];
}

int RBUF_EMPTY(const RBUF_T *rbuf) {
	assert_consistent(rbuf);
	return rbuf->l == 0;
}

int RBUF_FULL(const RBUF_T *rbuf) {
	assert_consistent(rbuf);
	return rbuf->l == RBUF_SIZE;
}
