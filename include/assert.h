#pragma once

#include <kernel.h>
#include <io_server.h>

#define STRINGIFY2(STR) #STR
#define STRINGIFY1(STR) STRINGIFY2(STR)

#define ASSERT(stmt) {\
    if (!(stmt)) { \
        fputs(COM2_DEBUG, "ASSERTION FAILED (" __FILE__ ":" STRINGIFY1(__LINE__) ") : " STRINGIFY2(stmt) EOL); \
        exitk(); \
    }}
