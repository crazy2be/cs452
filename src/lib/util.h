#pragma once

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(x, y) (((x) > (y)) ? (x) : (y))

#define STRINGIFY2(STR) #STR
#define STRINGIFY1(STR) STRINGIFY2(STR)

#define offsetof(type, member) __builtin_offsetof(type, member)
#define ARRAY_LENGTH(x) (sizeof(x)/sizeof(*x))

void exited_main(void);

void sleep(int n);
void *memset(void *ptr, int value, int num);
typedef unsigned size_t;
void* memcpy(void *dst, const void *src, size_t len);

int modi(int a, int b);
int strlen(const char *s);
char* strcpy(char *dst, const char *src);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, int n);

static inline int usermode(void) {
	unsigned cpsr;
	__asm__ ("mrs %0, cpsr" : "=r"(cpsr));
	return (cpsr & 0x1f) == 0x10;
}
