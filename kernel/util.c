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

void* memcpy(void *dst, const void *src, size_t len) {
    unsigned char *p = (unsigned char*) dst;
    while (len--) {
        *p++ = *(unsigned char*)src++;
    }
    return dst;
}

int modi(int a, int b) {
	return ((a % b) + b) % b;
}

int strlen(const char *s) {
    const char *p = s;
    while (*p++);
    return p - s - 1;
}

char* strcpy(char *dst, const char *src) {
    char *p = dst;
    while ((*p++ = *src++));
    return dst;
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a - *b;
}
