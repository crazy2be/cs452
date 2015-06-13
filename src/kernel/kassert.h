#pragma once

#include <util.h>

// this panics, and stops execution of the kernel, if the assertion fails
#define KFASSERT(stmt,fmt,...) {\
    if (!(stmt)) { \
        io_puts(COM2, "KERNEL ASSERTION FAILED (" __FILE__ ":" STRINGIFY1(__LINE__) ") : " STRINGIFY2(stmt) EOL); \
        io_printf(COM2, fmt EOL, ##__VA_ARGS__); \
        io_flush(COM2); \
        __asm__ __volatile__ ("b exited_main"); /* just stop execution in the kernel */ \
    } }

#define KASSERT(stmt) KFASSERT(stmt,"No details provided")
