#include <kernel.h>
#include "init_task.h"
#include "io.h"
#include "util.h"
#include "least_significant_set_bit.h"

#define STRINGIFY2(STR) #STR
#define STRINGIFY1(STR) STRINGIFY2(STR)

#define ASSERT(stmt) {\
    if (!(stmt)) { \
        io_puts(COM2, "ASSERTION FAILED (" __FILE__ ":" STRINGIFY1(__LINE__) ") : " STR1(stmt) EOL); \
        io_flush(COM2); \
    } }


void child(void) {
    printf("Child task %d" EOL, tid());
    io_flush(COM2);
}

void nop(void) {
}

const int init_task_priority = PRIORITY_MAX;

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
    printf("init task priority is %d" EOL, init_task_priority);
    io_flush(COM2);
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
