#include "util.h"

#include "io.h"

void assert(int musttrue, const char *msg) {
	if (musttrue) return;
	io_flush(COM2);
	io_puts(COM2, msg);
	io_flush(COM2);
}

void sleep(int n) {
	for (volatile int i = 0; i < n*10000; i++);
}

void *memset(void *ptr, int value, int num) {
	for (int i = 0; i < num; i++) {
		*((char*)ptr + i) = value;
	}
	return ptr;
}

void memcopy(void *dst, void *src, unsigned len) {
    while (len--) {
        *(unsigned char*)dst++ = *(unsigned char*)src++;
    }
}

int modi(int a, int b) {
	return ((a % b) + b) % b;
}
