#pragma once

#define PRIORITY_MAX 0
#define PRIORITY_MIN 31
#define PRIORITY_COUNT (PRIORITY_MIN + 1)

#ifdef QEMU
#define EOL "\n"
#else
#define EOL "\n\r"
#endif
