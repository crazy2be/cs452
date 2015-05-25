#include <kernel.h>
#include <io.h>

// lurky, but we have to include these to test stuff
#include "../kernel/least_significant_set_bit.h"
#include "../kernel/hashtable.h"
#include "../kernel/prng.h"
#include "../kernel/util.h"

#define STRINGIFY2(STR) #STR
#define STRINGIFY1(STR) STRINGIFY2(STR)

#define ASSERT(stmt) {\
    if (!(stmt)) { \
        io_puts(COM2, "ASSERTION FAILED (" __FILE__ ":" STRINGIFY1(__LINE__) ") : " STRINGIFY2(stmt) EOL); \
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
        pass();
        ASSERT(i == least_significant_set_bit(0x1 << i));
    }
}

struct Msg {
	int foo;
	int bar;
};
struct Reply {
	int foo2;
	int bar2;
};
static int receiving_tid = -1;
void sending_task(void) {
	for (int i = 0; i < 10; i++) {
		struct Msg msg = {0xFFffffff, 0x55555555};
		struct Reply rep;
		printf("Sending %d %d\n", msg.foo, msg.bar);
		send(receiving_tid, &msg, sizeof(msg), &rep, sizeof(rep));
		printf("Sent %d %d, got %d %d\n", msg.foo, msg.bar, rep.foo2, rep.bar2);
	}
}
void receiving_task(void) {
	for (int i = 0; i < 10; i++) {
		struct Msg msg;
		int tid;
		receive(&tid, &msg, sizeof(msg));
		struct Reply rep = {0x74546765, 0x45676367};
		printf("Got %d %d, replying with %d %d\n", msg.foo, msg.bar, rep.foo2, rep.bar2);
		reply(tid, &rep, sizeof(reply));
		printf("Replied\n");
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
    memcpy_tests();
    ASSERT(1);
    ASSERT(create(-1, child) == CREATE_INVALID_PRIORITY);
    ASSERT(create(32, child) == CREATE_INVALID_PRIORITY);
	printf("Creating messaging tasks...\n");
	receiving_tid = create(PRIORITY_MIN, receiving_task);
	int sending_tid = create(PRIORITY_MIN, sending_task);
	printf("Created tasks %d %d, init task exiting...\n", sending_tid, receiving_tid);
//     int i;
//     for (i = 0; i < 10; i++) {
//         // create tasks in descending priority order
//         create(PRIORITY_MIN - i, &child);
//     }

//     for (; i < 255; i++) {
//         create(PRIORITY_MIN, &nop);
//     }
//
//     ASSERT(create(4, child) == CREATE_INSUFFICIENT_RESOURCES);
}

int main(int argc, char *argv[]) {
    boot(init_task, PRIORITY_MAX);
}
