#pragma once

#include <util.h>
#include <io.h>
#include "qemu.h"

// this panics, and stops execution of the kernel, if the assertion fails
#define KASSERTF(stmt,fmt,...) {\
	if (!(stmt)) { \
		kprintf("%s: " fmt EOL, "KERNEL ASSERTION FAILED (" __FILE__ ":" STRINGIFY1(__LINE__) ") : " STRINGIFY2(stmt) EOL, ##__VA_ARGS__); \
		for (;;); \
	}}

#define KASSERT(stmt) KASSERTF(stmt, "No details provided")
#define KFASSERT KASSERTF // TODO: Unify these
