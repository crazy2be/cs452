#include <kernel.h>

int create(int priority, void *code) {
    __asm__ __volatile__ (SYSCALL(SYSCALL_CREATE));
}

int tid(void) {
    __asm__ __volatile__ (SYSCALL(SYSCALL_TID));
}

int parent_tid(void) {
    __asm__ __volatile__ (SYSCALL(SYSCALL_PARENT_TID));
}

void pass(void) {
    __asm__ __volatile__ (SYSCALL(SYSCALL_PASS));
}

void exitk(void) {
    __asm__ __volatile__ (SYSCALL(SYSCALL_EXIT));
}
