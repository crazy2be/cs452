#include <util.h>
#include "kassert.h"
/* #include "io.h" */
#include <io_server.h>

// printf buffers internally to a buffer allocated on the stack, and flushes this
// buffer with puts when it fills up, since putc incurs a lot of overhead to only
// transfer a single byte.

#define PRINTF_BUFSZ 80

typedef int (*producer)(char, void*);

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

static int bwui2a(producer produce, void *produce_state, unsigned int num, unsigned int base, int padding, char padchar, int negative) {
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
			if (produce('-', produce_state)) return 1;
		}
	}

	while (padding-- > 0) {
		if (produce(padchar, produce_state)) return 1;
	}

	if (negative && padchar != '0') {
		if (produce('-', produce_state)) return 1;
	}

	while (d != 0) {
		dgt = num / d;
		num %= d;
		d /= base;
		if (dgt > 0 || d == 0) {
			if (produce(dgt + (dgt < 10 ? '0' : 'a' - 10), produce_state)) return 1;
		}
	}

	return 0;
}

static int bwi2a(producer produce, void *produce_state, int num, int padding, char padchar) {
	int negative = num < 0;
	if (negative) {
		num = -num;
	}
	return bwui2a(produce, produce_state, num, 10, padding, padchar, negative);
}

#include "vargs.h"

static void format(producer produce, void* produce_state, const char *fmt, va_list va) {
	char ch, lz;
	int w;

	while ((ch = *(fmt++))) {
		if (ch != '%') {
			if (produce(ch, produce_state)) return;
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
				if (produce(va_arg(va, char), produce_state)) return;
				break;
			case 's':
				{
					char *str = va_arg(va, char*);
					while (*str) {
						if(produce(*str++, produce_state)) return;
					}
				}
				break;
			case 'u':
				if (bwui2a(produce, produce_state, va_arg(va, unsigned int), 10, w, lz, 0)) return;
				break;
			case 'x':
				if (bwui2a(produce, produce_state, va_arg(va, unsigned int), 16, w, lz, 0)) return;
				break;
			case 'd':
				if (bwi2a(produce, produce_state, va_arg(va, int), w, lz)) return;
				break;
			case '%':
				if (produce(ch, produce_state)) return;
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

static int produce_buffered(char c, void *s) {
	struct produce_buffered_state *state = (struct produce_buffered_state*) s;
	state->buf[state->i++] = c;
	if (state->i == PRINTF_BUFSZ - 1) flush_buffer(state);
	return 0;
}

int fprintf(int channel, const char *fmt, ...) {
	va_list va;
	va_start(va,fmt);
	struct produce_buffered_state buf = { .i = 0, .channel = channel };
	format(produce_buffered, (void*) &buf, fmt, va);
	flush_buffer(&buf);
	va_end(va);
	return -1; // Meh
}

struct produce_string_state {
	unsigned n;
	char *buf;
};

static int produce_string(char c, void *s) {
	struct produce_string_state *state = (struct produce_string_state*) s;
	if (state->n > 0) {
		*state->buf++ = c;
		state->n--;
		return 0;
	} else {
		return 1;
	}
}

int snprintf(char *buf, unsigned size, const char *fmt, ...) {
	va_list va;
	va_start(va,fmt);
	struct produce_string_state state = {size, buf};
	format(produce_string, &state, fmt, va);
	produce_string('\0', &state);
	va_end(va);
	return 0; // we should probably return something meaningful
}
