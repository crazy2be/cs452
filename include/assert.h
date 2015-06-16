#pragma once

#include <kernel.h>
#include <io_server.h>

#define STRINGIFY2(STR) #STR
#define STRINGIFY1(STR) STRINGIFY2(STR)

#define ASSERTF(stmt,fmt,...) {\
    if (!(stmt)) { \
		kprintf("%s:" fmt EOL, "ASSERTION FAILED (" __FILE__ ":" STRINGIFY1(__LINE__) ") : " STRINGIFY2(stmt) EOL, ##__VA_ARGS__); \
		exitk(); \
    }}

#define ASSERT(stmt) ASSERTF(stmt, "No details provided")
