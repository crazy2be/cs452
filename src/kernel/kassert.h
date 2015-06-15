#pragma once

#include <util.h>
#include <io_server.h>
#include "qemu.h"

// this panics, and stops execution of the kernel, if the assertion fails
#define KFASSERT(stmt,fmt,...) {\
    if (!(stmt)) { \
        kprintf("%s: " fmt EOL, "KERNEL ASSERTION FAILED (" __FILE__ ":" STRINGIFY1(__LINE__) ") : " STRINGIFY2(stmt) EOL, ##__VA_ARGS__); \
		exited_main(); \
    } }

#define KASSERT(stmt) KFASSERT(stmt,"No details provided")
