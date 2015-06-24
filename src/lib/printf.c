#include <util.h>
#include <io_server.h>
#include "vargs.h"

// printf buffers internally to a buffer allocated on the stack, and flushes this
// buffer with puts when it fills up, since putc incurs a lot of overhead to only
// transfer a single byte.

#define PRINTF_BUFSZ 80

typedef void (*producer)(char);

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

static void bwui2a(producer produce, unsigned int num,
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
			produce('-');
		}
	}

	while (padding-- > 0) {
		produce(padchar);
	}

	if (negative && padchar != '0') {
		produce('-');
	}

	while (d != 0) {
		dgt = num / d;
		num %= d;
		d /= base;
		produce(dgt + (dgt < 10 ? '0' : 'a' - 10));
	}
}

static void bwi2a(producer produce, int num, int padding, char padchar) {
	int negative = num < 0;
	if (negative) {
		num = -num;
	}
	bwui2a(produce, num, 10, padding, padchar, negative);
}

static void format(producer produce, const char *fmt, va_list va) {
	char ch, lz;
	int w;

	while ((ch = *(fmt++))) {
		if (ch != '%') {
			produce(ch);
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
				produce(va_arg(va, char));
				break;
			case 's':
				{
					char *str = va_arg(va, char*);
					while (*str) {
						produce(*str++);
					}
				}
				break;
			case 'u':
				bwui2a(produce, va_arg(va, unsigned int), 10, w, lz, 0);
				break;
			case 'x':
				bwui2a(produce, va_arg(va, unsigned int), 16, w, lz, 0);
				break;
			case 'd':
				bwi2a(produce, va_arg(va, int), w, lz);
				break;
			case '%':
				produce(ch);
				break;
			}
		}
	}
}

static void flush_buffer(int channel, char *buf, int i) {
	if (i != 0) {
		buf[i] = '\0';
		fputs(channel, buf);
	}
}

int fprintf(int channel, const char *fmt, ...) {
	va_list va;
	va_start(va,fmt);

	char buf[PRINTF_BUFSZ];
	int i = 0;
	int bytes_printed = 0;

	void produce_buffered(char c) {
		bytes_printed++;
		buf[i++] = c;
		if (i == PRINTF_BUFSZ - 1) {
			flush_buffer(channel, buf, i);
			i = 0;
		}

	}

	format(produce_buffered, fmt, va);
	flush_buffer(channel, buf, i);

	va_end(va);

	return bytes_printed;
}

int snprintf(char *buf, unsigned size, const char *fmt, ...) {
	va_list va;
	va_start(va,fmt);
	int bytes_printed = 0;

	void produce_string(char c) {
		bytes_printed++;
		if (size > 1) {
			*buf++ = c;
			--size;
		}
	}

	format(produce_string, fmt, va);
	va_end(va);

	*buf = '\0';
	return bytes_printed;
}
