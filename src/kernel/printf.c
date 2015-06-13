#include <util.h>
#include "kassert.h"
/* #include "io.h" */
#include <io_server.h>

// printf buffers internally to a buffer allocated on the stack, and flushes this
// buffer with puts when it fills up, since putc incurs a lot of overhead to only
// transfer a single byte.

#define PRINTF_BUFSZ 80

static int flush_buffer(int channel, char *buf, int i) {
	buf[i] = '\0';
	puts(channel, buf);
	return 0;
}

static int produce(int channel, char *buf, int i, char c) {
	buf[i++] = c;
	if (i == PRINTF_BUFSZ - 1) return flush_buffer(channel, buf, i);
	return i;
}

static int bwa2d(char ch) {
	if (ch >= '0' && ch <= '9') return ch - '0';
	if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
	if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
	return -1;
}

static char bwa2i(char ch, char **src, int base, int *nump) {
	int num, digit;
	char *p;

	p = *src; num = 0;
	while ((digit = bwa2d(ch)) >= 0) {
		if (digit > base) break;
		num = num*base + digit;
		ch = *p++;
	}
	*src = p; *nump = num;
	return ch;
}

static int bwui2a(int channel, char *buf, int i, unsigned int num, unsigned int base, int padding, char padchar, int negative) {
	/* int n = 0; */
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
			i = produce(channel, buf, i, '-');
		}
	}

	while (padding-- > 0) i = produce(channel, buf, i, padchar);

	if (negative && padchar != '0') {
		i = produce(channel, buf, i, '-');
	}

	while (d != 0) {
		dgt = num / d;
		num %= d;
		d /= base;
		if (dgt > 0 || d == 0) {
			i = produce(channel, buf, i, dgt + (dgt < 10 ? '0' : 'a' - 10));
			/* ++n; */
		}
	}
	return i;
}

static int bwi2a(int channel, char* buf, int i, int num, int padding, char padchar) {
	int negative = num < 0;
	if (negative) {
		num = -num;
	}
	return bwui2a(channel, buf, i, num, 10, padding, padchar, negative);
}

#include "vargs.h"

static void format(int channel, char *fmt, va_list va) {
	char buf[PRINTF_BUFSZ];
	int i = 0;
	char ch, lz;
	int w;

	while ((ch = *(fmt++))) {
		if (ch != '%') {
			i = produce(channel, buf, i, ch);
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
				i = produce(channel, buf, i, va_arg(va, char));
				break;
			case 's':
				i = flush_buffer(channel, buf, i);
				puts(channel, va_arg(va, char*));
				break;
			case 'u':
				i = bwui2a(channel, buf, i, va_arg(va, unsigned int), 10, w, lz, 0);
				break;
			case 'x':
				i = bwui2a(channel, buf, i, va_arg(va, unsigned int), 16, w, lz, 0);
				break;
			case 'd':
				i = bwi2a(channel, buf, i, va_arg(va, int), w, lz);
				break;
			case '%':
				i = produce(channel, buf, i, ch);
				break;
			}
		}
	}
	flush_buffer(channel, buf, i);
}

int printf(int channel, const char *fmt, ...) {
	va_list va;
	va_start(va,fmt);
	format(channel, (char*)fmt, va); // Shouldn't need this cast...
	va_end(va);
	return -1; // Meh
}
