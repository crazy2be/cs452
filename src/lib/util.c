#include "util.h"

void sleep(int n) {
	for (volatile int i = 0; i < n*10000; i++);
}

void *memset(void *ptr, int value, int num) {
	for (int i = 0; i < num; i++) {
		*((char*)ptr + i) = value;
	}
	return ptr;
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
