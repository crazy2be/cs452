#include <kernel.h>
#include <io.h>

// lurky, but we have to include these to test stuff
#include "../kernel/least_significant_set_bit.h"
#include "../kernel/util.h"

#define STRINGIFY2(STR) #STR
#define STRINGIFY1(STR) STRINGIFY2(STR)

#define ASSERT(stmt) {\
    if (!(stmt)) { \
        io_puts(COM2, "ASSERTION FAILED (" __FILE__ ":" STRINGIFY1(__LINE__) ") : " STR1(stmt) EOL); \
        io_flush(COM2); \
    } }


void child(void) {
    io_printf(COM2, "Child task %d" EOL, tid());
    io_flush(COM2);
}

void nop(void) {
}

void memcpy_tests(void) {
    const unsigned bufsz = 80;
    unsigned char buf_a[bufsz], buf_b[bufsz];
    unsigned i, offset_a, offset_b, len;

    for (i = 0; i < bufsz; i++) {
        buf_a[i] = i;
    }

    for (offset_a = 0; offset_a < 3; offset_a++) {
        for (offset_b = 0; offset_b < 3; offset_b++) {
            len = bufsz - ((offset_a > offset_b) ? offset_a : offset_b);
            memcopy(buf_b + offset_b, buf_a + offset_a, len);
            for (i = 0; i < len; i++) {
                ASSERT(buf_b[i + offset_b] == i + offset_a);
            }
        }
    }

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
    memcpy_tests();
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
