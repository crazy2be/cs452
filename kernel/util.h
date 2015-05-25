#pragma once

void assert(int musttrue, const char *msg);
void sleep(int n);
void *memset(void *ptr, int value, int num);
// non-traditional name b/c of conflict with built-in function
// TODO: I should just configure the emulator build script to link in memcpy from libc
void memcopy(void *dst, void *src, unsigned len);

int modi(int a, int b);
int strlen(const char *s);
char* strcpy(char *dst, const char *src);
int strcmp(const char *a, const char *b);
