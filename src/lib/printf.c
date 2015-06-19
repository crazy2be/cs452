#include <util.h>
#include <io_server.h>
#include "vargs.h"

// printf buffers internally to a buffer allocated on the stack, and flushes this
// buffer with puts when it fills up, since putc incurs a lot of overhead to only
// transfer a single byte.

#define PRINTF_BUFSZ 80

typedef void (*producer)(char, void*);

static int bwa2d(char ch) {
	if (ch >= '0' && ch <= '9') return ch - '0';
	if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
	if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
	return -1;
}

static char bwa2i(char ch, const char **src, int base, int *nump) {
	int num = 0;
	int digit;
	const char *p = *src;

	while ((digit = bwa2d(ch)) >= 0) {
		if (digit > base) break;
		num = num*base + digit;
		ch = *p++;
	}

	*src = p; *nump = num;
	return ch;
}

static void bwui2a(producer produce, void *produce_state, unsigned int num,
		unsigned int base, int padding, char padchar, int negative) {
	int dgt;
	unsigned int d = 1;

	padding--;

	// figure out the biggest digit & the amount of padding required
	while ((num / d) >= base) {
		d *= base;
		padding--;
	}

	if (negative) {
		padding--;
		// only put the negative sign before padding if padding with zeros
		if (padchar == '0') {
			produce('-', produce_state);
		}
	}

	while (padding-- > 0) {
		produce(padchar, produce_state);
	}

	if (negative && padchar != '0') {
		produce('-', produce_state);
	}

	while (d != 0) {
		dgt = num / d;
		num %= d;
		d /= base;
		if (dgt > 0 || d == 0) {
			produce(dgt + (dgt < 10 ? '0' : 'a' - 10), produce_state);
		}
	}
}

static void bwi2a(producer produce, void *produce_state, int num, int padding, char padchar) {
	int negative = num < 0;
	if (negative) {
		num = -num;
	}
	bwui2a(produce, produce_state, num, 10, padding, padchar, negative);
}

static void format(producer produce, void* produce_state, const char *fmt, va_list va) {
	char ch, lz;
	int w;

	while ((ch = *(fmt++))) {
		if (ch != '%') {
			produce(ch, produce_state);
		} else {
			lz = ' ';
			w = 0;
			ch = *(fmt++);
			switch (ch) {
			case '0':
				lz = '0';
				ch = *(fmt++);
			case '1'...'9':
				ch = bwa2i(ch, &fmt, 10, &w);
				break;
			}
			switch (ch) {
			case 0:
				return;
			case 'c':
				produce(va_arg(va, char), produce_state);
				break;
			case 's':
				{
					char *str = va_arg(va, char*);
					while (*str) {
						produce(*str++, produce_state);
					}
				}
				break;
			case 'u':
				bwui2a(produce, produce_state, va_arg(va, unsigned int), 10, w, lz, 0);
				break;
			case 'x':
				bwui2a(produce, produce_state, va_arg(va, unsigned int), 16, w, lz, 0);
				break;
			case 'd':
				bwi2a(produce, produce_state, va_arg(va, int), w, lz);
				break;
			case '%':
				produce(ch, produce_state);
				break;
			}
		}
	}
}

struct produce_buffered_state {
	int channel;
	char buf[PRINTF_BUFSZ];
	int i;
};

static void flush_buffer(struct produce_buffered_state *state) {
	if (state->i != 0) {
		state->buf[state->i] = '\0';
		fputs(state->channel, state->buf);
		state->i = 0;
	}
}

static void produce_buffered(char c, void *s) {
	struct produce_buffered_state *state = (struct produce_buffered_state*) s;
	state->buf[state->i++] = c;
	if (state->i == PRINTF_BUFSZ - 1) flush_buffer(state);
}

int fprintf(int channel, const char *fmt, ...) {
	va_list va;
	va_start(va,fmt);
	struct produce_buffered_state buf = { .i = 0, .channel = channel };
	format(produce_buffered, (void*) &buf, fmt, va);
	flush_buffer(&buf);
	va_end(va);

	//TODO: to be consistent with the usual printf return, we should return the number of characters written
	return -1; // Meh
}

struct produce_string_state {
	unsigned remaining;
	unsigned total; // number of characters that *would* have been written, given sufficient space
	char *buf;
};

static void produce_string(char c, void *s) {
	struct produce_string_state *state = (struct produce_string_state*) s;
	state->total++;
	if (state->remaining > 0) {
		*state->buf++ = c;
		state->remaining--;
	}
}

int snprintf(char *buf, unsigned size, const char *fmt, ...) {
	va_list va;
	va_start(va,fmt);
	struct produce_string_state state = {size, 0, buf};
	format(produce_string, &state, fmt, va);
	va_end(va);

	if (state.remaining == 0) {
		state.buf--; // overwrite the last character in order to null-terminate
	}

	*state.buf = '\0';
	return state.total;
}
