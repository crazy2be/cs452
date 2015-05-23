#include <kernel.h>
#include <bwio.h>

// lurky, but we have to include these to test stuff
#include "../kernel/least_significant_set_bit.h"

#define STRINGIFY2(STR) #STR
#define STRINGIFY1(STR) STRINGIFY2(STR)

#define ASSERT(stmt) {\
    if (!(stmt)) { \
        bwputstr(COM2, "ASSERTION FAILED (" __FILE__ ":" STRINGIFY1(__LINE__) ") : " STR1(stmt) EOL); \
    } }


void child(void) {
    bwprintf(COM2, "Child task %d" EOL, tid());
}

void nop(void) {
}

void lssb_tests(void) {
    int i;
    ASSERT(0 == least_significant_set_bit(0xffffffff));
    ASSERT(2 == least_significant_set_bit(0xc));
    for (i = 0; i < 32; i++) {
        ASSERT(i == least_significant_set_bit(0x1 << i));
    }
}

void init_task(void) {
    lssb_tests();
    ASSERT(1);
    ASSERT(create(-1, child) == CREATE_INVALID_PRIORITY);
    ASSERT(create(32, child) == CREATE_INVALID_PRIORITY);
    int i;
    for (i = 0; i < 10; i++) {
        // create tasks in descending priority order
        create(PRIORITY_MIN - i, &child);
    }

    for (; i < 255; i++) {
        create(PRIORITY_MIN, &nop);
    }

    ASSERT(create(4, child) == CREATE_INSUFFICIENT_RESOURCES);
}

int main(int argc, char *argv[]) {
    boot(init_task, PRIORITY_MAX);
}
