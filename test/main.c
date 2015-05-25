#include <kernel.h>
#include <io.h>

// lurky, but we have to include these to test stuff
#include "../kernel/least_significant_set_bit.h"
#include "../kernel/hashtable.h"
#include "../kernel/prng.h"

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

void lssb_tests(void) {
    int i;
    ASSERT(0 == least_significant_set_bit(0xffffffff));
    ASSERT(2 == least_significant_set_bit(0xc));
    for (i = 0; i < 32; i++) {
        pass();
        ASSERT(i == least_significant_set_bit(0x1 << i));
    }
}

void hashtable_tests(void) {
    struct prng gen;
    struct hashtable ht;
    int val, i;
    char buf[MAX_KEYLEN + 1];
    hashtable_init(&ht);
    ASSERT(HASHTABLE_SUCCESS == hashtable_set(&ht, "foo", 7));
    ASSERT(HASHTABLE_SUCCESS == hashtable_get(&ht, "foo", &val));
    ASSERT(7 == val);
    ASSERT(HASHTABLE_KEY_NOT_FOUND == hashtable_get(&ht, "bar", &val));

    // stress test test of insertions
    hashtable_init(&ht);
    const int reps = 255;
    const unsigned seed = 0xab32719c;
    prng_init(&gen, seed);
    for (i = 0; i < reps; i++) {
        prng_gens(&gen, buf, MAX_KEYLEN + 1);

        // assert that there are no collisions with our key generation
        ASSERT(HASHTABLE_KEY_NOT_FOUND == hashtable_get(&ht, buf, &val));

        ASSERT(HASHTABLE_SUCCESS == hashtable_set(&ht, buf, i));
    }

    prng_init(&gen, seed);
    for (i = 0; i < reps; i++) {
        prng_gens(&gen, buf, MAX_KEYLEN + 1);

        ASSERT(HASHTABLE_SUCCESS == hashtable_get(&ht, buf, &val));
        ASSERT(i == val);
    }
}

void init_task(void) {
    lssb_tests();
    hashtable_tests();
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
