#pragma once

// this panics, and stops execution of the kernel, if the assertion fails
#define KASSERT(stmt) {\
    if (!(stmt)) { \
        io_puts(COM2, "KERNEL ASSERTION FAILED (" __FILE__ ":" STRINGIFY1(__LINE__) ") : " STRINGIFY2(stmt) EOL); \
        io_flush(COM2); \
        __asm__ __volatile__ ("b exited_main"); /* just stop execution in the kernel */ \
    } }
