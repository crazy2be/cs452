#pragma once

#include <kernel.h>
#include <io.h>

#define STRINGIFY2(STR) #STR
#define STRINGIFY1(STR) STRINGIFY2(STR)

#define ASSERT(stmt) {\
    if (!(stmt)) { \
        io_puts(COM2, "ASSERTION FAILED (" __FILE__ ":" STRINGIFY1(__LINE__) ") : " STRINGIFY2(stmt) EOL); \
        io_flush(COM2); \
        exitk(); \
    }}
/* Can be handy when debugging failures.
 	else { \
 		io_puts(COM2, "ASERTION PASSED (" __FILE__ ":" STRINGIFY1(__LINE__) ") : " STRINGIFY2(stmt) EOL); \
 		io_flush(COM2); \
 	}}*/
