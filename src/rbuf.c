#include "rbuf.h"
#ifdef TEST
#include <assert.h>
#else
extern void assert(int musttrue, const char *msg);
#define assert(a) assert(a, "assertion failed! " #a);
#endif

void rbuf_init(struct RBuf *rbuf) {
	rbuf->i = 0;
	rbuf->l = 0;
	for (int i = 0; i < RBUF_SIZE; i++) rbuf->buf[i] = 0;
}

void rbuf_put(struct RBuf *rbuf, char val) {
	//assert(rbuf->i < RBUF_SIZE);
	//assert(rbuf->l + 1 <= RBUF_SIZE);
	rbuf->buf[(rbuf->i + rbuf->l) % RBUF_SIZE] = val;
	rbuf->l++;
}

char rbuf_take(struct RBuf *rbuf) {
	//assert(rbuf->l > 0);
	//assert(rbuf->i >= 0 && rbuf->i < RBUF_SIZE);
	char val = rbuf->buf[rbuf->i];
	rbuf->i = (rbuf->i + 1) % RBUF_SIZE;
	rbuf->l--;
	return val;
}
