#include <kernel.h>

// Note: this business of returning tid explicitly as a register variable
// is pretty stupid, but cowan's compiler doesn't think I've provided the
// return value otherwise.
// This explicit return statement is optimized away, and actually doesn't
// cause any instructions to be ommitted - it is purely to soothe the compiler.
//
// The reason I'm doing this in C at all, and not directly in assembly to avoid
// this is that I want to be able to use the C macros for the syscall IDs.

int create(int priority, void *code) {
    register int tid __asm__("r0");
    __asm__ __volatile__ (SYSCALL(SYSCALL_CREATE));
    return tid;
}

int tid(void) {
    register int tid __asm__("r0");
    __asm__ __volatile__ (SYSCALL(SYSCALL_TID));
    return tid;
}

int parent_tid(void) {
    register int tid __asm__("r0");
    __asm__ __volatile__ (SYSCALL(SYSCALL_PARENT_TID));
    return tid;
}

void pass(void) {
    __asm__ __volatile__ (SYSCALL(SYSCALL_PASS));
}

void exitk(void) {
    __asm__ __volatile__ (SYSCALL(SYSCALL_EXIT));
	for (;;) {} // Sooth the compiler. We should never get here.
}
