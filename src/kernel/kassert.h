#pragma once

#include <util.h>
#include <io_server.h>

// this panics, and stops execution of the kernel, if the assertion fails
#define KASSERT(stmt) {\
	if (!(stmt)) { \
		kprintf("KERNEL ASSERTION FAILED (" __FILE__ ":" STRINGIFY1(__LINE__) ") : " STRINGIFY2(stmt) EOL); \
		__asm__ __volatile__ ("b exited_main"); /* just stop execution in the kernel */ \
		__builtin_unreachable(); \
	} }
