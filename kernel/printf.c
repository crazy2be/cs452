#include <util.h>
#include "kassert.h"
#include "io.h"

static void bwputc(int channel, char c) {
	io_putc(channel, c);
}

static void bwputw(int channel, int n, char fc, char *bf) {
	char ch;
	char *p = bf;
	while (*p++ && n > 0) n--;
	while (n-- > 0) bwputc(channel, fc);
	while ((ch = *bf++)) bwputc(channel, ch);
}

static int bwa2d(char ch) {
	if (ch >= '0' && ch <= '9') return ch - '0';
	if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
	if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
	KASSERT(0 && "This should be unreachable");
	return 1;
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

static void bwui2a(unsigned int num, unsigned int base, char *bf) {
	int n = 0;
	int dgt;
	unsigned int d = 1;

	while ((num / d) >= base) d *= base;
	while (d != 0) {
		dgt = num / d;
		num %= d;
		d /= base;
		if (n || dgt > 0 || d == 0) {
			*bf++ = dgt + (dgt < 10 ? '0' : 'a' - 10);
			++n;
		}
	}
	*bf = 0;
}

static void bwi2a(int num, char *bf) {
	if (num < 0) {
		num = -num;
		*bf++ = '-';
	}
	bwui2a(num, 10, bf);
}

#include "vargs.h"

static void bwformat(int channel, char *fmt, va_list va) {
	char bf[12];
	char ch, lz;
	int w;

	while ((ch = *(fmt++))) {
		if (ch != '%') bwputc(channel, ch);
		else {
			lz = 0; w = 0;
			ch = *(fmt++);
			switch (ch) {
			case '0': lz = 1; ch = *(fmt++); break;
			case '1'...'9': ch = bwa2i(ch, &fmt, 10, &w); break;
			}
			switch (ch) {
			case 0: return;
			case 'c': bwputc(channel, va_arg(va, char)); break;
			case 's': bwputw(channel, w, 0, va_arg(va, char*)); break;
			case 'u':
				bwui2a(va_arg(va, unsigned int), 10, bf);
				bwputw(channel, w, lz, bf); break;
			case 'd':
				bwi2a(va_arg(va, int), bf);
				bwputw(channel, w, lz, bf); break;
			case 'x':
				bwui2a(va_arg(va, unsigned int), 16, bf);
				bwputw(channel, w, lz, bf); break;
			case '%':
				bwputc(channel, ch); break;
			}
		}
	}
}

int io_printf(int channel, const char *fmt, ...) {
	va_list va;
	va_start(va,fmt);
	bwformat(channel, (char*)fmt, va); // Shouldn't need this cast...
	va_end(va);
	return -1; // Meh
}

// GCC is clever about printf, and replaces it with puts automatically if it
// determines it can do so without changing behaviour. So we un-clever it here,
// in order to avoid linker errors.
int puts(const char *s) {
	return printf(s);
}
