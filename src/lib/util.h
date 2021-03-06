#pragma once

#define NULL (void*)0
#if 0 // For cowan's compiler
typedef int bool;
#define false 0
#define true 1
#else
#include <stdbool.h>
#include <stdint.h>
#endif

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(x, y) (((x) > (y)) ? (x) : (y))

#define STRINGIFY2(STR) #STR
#define STRINGIFY1(STR) STRINGIFY2(STR)

/* we require a second level of indirection here in order to have the
 * parameters expanded before concatenating - see
 * https://stackoverflow.com/questions/1489932/c-preprocessor-and-concatenation */
#define PASTER2(x, y) x##_##y
#define PASTER(x, y) PASTER2(x, y)

#define offsetof(type, member) __builtin_offsetof(type, member)
#define ARRAY_LENGTH(x) (sizeof(x)/sizeof(*x))

void exited_main(void);

void sleep(int n);
void *memset(void *ptr, int value, int num);
#define memzero(ptr) memset(ptr, 0, sizeof(*(ptr)))
typedef unsigned size_t;
void* memcpy(void *dst, const void *src, size_t len);

int modi(int a, int b);
int strlen(const char *s);
char* strcpy(char *dst, const char *src);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, int n);
unsigned sqrti(unsigned n);

static inline int usermode(void) {
	unsigned cpsr;
	__asm__ ("mrs %0, cpsr" : "=r"(cpsr));
	return (cpsr & 0x1f) == 0x10;
}

static inline int abs(int a) {
	return (a < 0) ? -a : a;
}

int powi(int base, unsigned exp);
