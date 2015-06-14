#pragma once

#include <util.h>
#include <io_server.h>
#include "qemu.h"

// this panics, and stops execution of the kernel, if the assertion fails
// TODO: after we get our own compiler working on the school machines,
// we can do this properly with __builtin_unreachable
#define KASSERT(stmt) {\
	if (!(stmt)) { \
		kprintf("KERNEL ASSERTION FAILED (" __FILE__ ":" STRINGIFY1(__LINE__) ") : " STRINGIFY2(stmt) EOL); \
		exited_main(); \
	} }
