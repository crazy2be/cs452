#include "util.h"

void sleep(int n) {
	for (volatile int i = 0; i < n*10000; i++);
}

void *memset(void *ptr, int value, int num) {
	// we expect the value to be a byte (but stored in an int for historical reasons)

	// We want to vectorize as much of the memset as possible, and set the memory
	// by writing a word at a time, instead of byte-by-byte.

	// We can only do vectorized copy to a word-aligned destination.
	// If the provided pointer isn't word-aligned, write bytes until it becomes
	// word aligned
	unsigned char * const max_c = ((unsigned char*) ptr) + num;
	unsigned char *ptr_c = (unsigned char*) ptr;
	unsigned char *word_aligned_begin = ptr_c + ((unsigned) ptr_c | 0x3);

	if (word_aligned_begin < max_c) {
		// copy the bytes before the start of the word-aligned segment
		while (ptr_c < word_aligned_begin) *ptr_c++ = value;

		unsigned *ptr_w = (unsigned*) ptr_c;
		unsigned * const max_w = ptr_w + (num / 4);
		// We pad the value with itself to prepare for the word by word copy:
		// 0x000000ab -> 0xabababab
		unsigned word_value = value | value << 8;
		word_value |= word_value << 16;
		while (ptr_w < max_w) *ptr_w++ = word_value;

		// now copy any bytes left over that can't be done with the word-aligned copy
		ptr_c = (unsigned char*) ptr_w;
	}

	while (ptr_c < max_c)  *ptr_c++ = value;

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

int strncmp(const char *a, const char *b, int n) {
	while (*a && *a == *b && n-- > 0) {
		a++;
		b++;
	}
	return n > 0 && *a - *b;
}
