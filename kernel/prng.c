#include "prng.h"

// mersenne twister prng implmentation
// ported from example given by https://en.wikipedia.org/wiki/Mersenne_Twister
// this is largely used to generate random data for test functions

void prng_init(struct prng *g, unsigned seed) {
	g->index = 0;
	g->buf[0] = seed;
	for (unsigned i = 1; i < PRNG_BUFSZ; i++) {
		unsigned last = g->buf[i - 1];
		g->buf[i] = 1812433253 * (last ^ (last >> 30)) + i;
	}
}

static void generate(struct prng *g) {
	for (unsigned i = 0; i < PRNG_BUFSZ; i++) {
		int y = (g->buf[i] & 0x80000000) | (g->buf[(i + 1) % PRNG_BUFSZ] & 0x7fffffff);
		int updated = g->buf[(i + 397) % PRNG_BUFSZ] ^ (y >> 1);
		if (y % 2) {
			updated ^= 0x9908b0df;
		}
		g->buf[i] = updated;
	}
}

unsigned prng_gen(struct prng *g) {
	if (g->index == PRNG_BUFSZ) {
		g->index = 0;
		generate(g);
	}

	unsigned y = g->buf[g->index++];

	y ^= y >> 11;
	y ^= (y << 7) & 0x9d2c5680;
	y ^= (y << 15) & 0xefc60000;
	y ^= (y >> 18);

	return y;
}

void prng_gens(struct prng *g, char *buf, unsigned len) {
	if (len == 0) return;
	buf[len--] = '\0';
	if (len == 0) return;

	for (;;) {
		unsigned rand = prng_gen(g);
		switch (len % 4) {
		case 0:
			*buf++ = (char) rand;
			rand = rand >> 8;
		case 3:
			*buf++ = (char) rand;
			rand = rand >> 8;
		case 2:
			*buf++ = (char) rand;
			rand = rand >> 8;
		case 1:
			*buf++ = (char) rand;
			rand = rand >> 8;
		}
		if (len <= 4) break;
		len -= 4;
	}
}
