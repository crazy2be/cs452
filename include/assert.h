#pragma once

#include <kernel.h>
#include <io_server.h>

#define STRINGIFY2(STR) #STR
#define STRINGIFY1(STR) STRINGIFY2(STR)

#define ASSERTF(stmt,fmt,...) ({ \
	int result = (stmt); \
	if (!result) { \
		kprintf("%s:" fmt EOL, \
		"ASSERTION FAILED (" __FILE__ ":" STRINGIFY1(__LINE__) ") : " \
		STRINGIFY2(stmt) EOL, ##__VA_ARGS__); \
		halt(); \
	} result; })

#define ASSERT(stmt) ASSERTF(stmt, "No details provided")
#define ASSERTOK(syscall) ({ \
		int result2 = (syscall); \
		ASSERTF(result2 >= 0, "Syscall '" STRINGIFY2(syscall) "' failed with code %d", result2); result2; })

#define WTF(fmt, ...) ASSERTF(false, fmt, ##__VA_ARGS__);
