#pragma once

#define NULL (void*)0

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(x, y) (((x) > (y)) ? (x) : (y))

void assert(int musttrue, const char *msg);
void sleep(int n);
void *memset(void *ptr, int value, int num);
// non-traditional name b/c of conflict with built-in function
// TODO: I should just configure the emulator build script to link in memcpy from libc
typedef unsigned size_t;
void* memcpy(void *dst, const void *src, size_t len);

int modi(int a, int b);
int strlen(const char *s);
char* strcpy(char *dst, const char *src);
int strcmp(const char *a, const char *b);
