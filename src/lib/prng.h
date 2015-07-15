#pragma once

#define PRNG_BUFSZ 624

struct prng {
	unsigned buf[PRNG_BUFSZ];
	unsigned index;
};

void prng_init(struct prng *g, unsigned seed);
unsigned prng_gen(struct prng *g);
void prng_gen_buf(struct prng *g, char *buf, unsigned len);
