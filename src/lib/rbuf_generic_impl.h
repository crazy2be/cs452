#pragma once
#include "util.h"

// TODO: it's a code smell that data structures asserting in a user task can cause
// the kernel to panic
#include "../kernel/kassert.h"

void RBUF_INIT(RBUF_T *rbuf) {
	rbuf->i = 0;
	rbuf->l = 0;
	/* for (int i = 0; i < RBUF_SIZE; i++) rbuf->buf[i] = 0; */
}

void RBUF_PUT(RBUF_T *rbuf, RBUF_ELE val) {
	KASSERT(rbuf->i < RBUF_SIZE);
	KASSERT(rbuf->l + 1 <= RBUF_SIZE);
	rbuf->buf[(rbuf->i + rbuf->l) % RBUF_SIZE] = val;
	rbuf->l++;
}

RBUF_ELE RBUF_TAKE(RBUF_T *rbuf) {
	KASSERT(rbuf->l > 0);
	KASSERT(rbuf->i >= 0 && rbuf->i < RBUF_SIZE);
	RBUF_ELE val = rbuf->buf[rbuf->i];
	rbuf->i = (rbuf->i + 1) % RBUF_SIZE;
	rbuf->l--;
	return val;
}
