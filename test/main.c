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
            memcpy(buf_b + offset_b, buf_a + offset_a, len);
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
	int a;
	int b;
};
struct Reply {
	int sum;
	int prod;
};

// this is pretty bad, but it's just for testing
static int receiving_tid = -1;
static int misbehaving_receiving_tid = -1;
static int nop_tid = -1;
static int producers = 20;

// webscale multiplication!
void sending_task(void) {
	for (int i = 0; i < 10; i++) {
		struct Msg msg;
		struct Reply rep;

        msg.a = i * 4;
        msg.b = i * 17 + 128;

        ASSERT(send(receiving_tid, &msg, sizeof(msg), &rep, sizeof(rep)) == sizeof(rep));
        ASSERT(rep.sum == msg.a + msg.b);
        ASSERT(rep.prod == msg.a * msg.b);
	}
}

void receiving_task(void) {
	for (int i = 0; i < producers * 10; i++) {
		struct Msg msg;
		struct Reply rep;
		int tid;

		ASSERT(receive(&tid, &msg, sizeof(msg)) == sizeof(msg));

        rep.sum = msg.a + msg.b;
        rep.prod = msg.a * msg.b;

		ASSERT(reply(tid, &rep, sizeof(rep)) == REPLY_SUCCESSFUL);
	}
    printf("Receive done" EOL);
}

void misbehaving_sending_task(void) {
    struct Msg msg;
    struct Reply rep;

    // check sending to impossible tasks
    ASSERT(send(-6, &msg, sizeof(msg), &rep, sizeof(rep)) == SEND_IMPOSSIBLE_TID);
    ASSERT(send(267, &msg, sizeof(msg), &rep, sizeof(rep)) == SEND_IMPOSSIBLE_TID);
    ASSERT(send(254, &msg, sizeof(msg), &rep, sizeof(rep)) == SEND_INVALID_TID);
    ASSERT(send(tid(), &msg, sizeof(msg), &rep, sizeof(rep)) == SEND_INVALID_TID);

    // our parent should have exited by now, so we shouldn't be able to send to it
    ASSERT(send(parent_tid(), &msg, sizeof(msg), &rep, sizeof(rep)) == SEND_INVALID_TID);

    ASSERT(send(misbehaving_receiving_tid, &msg, sizeof(msg), &rep, sizeof(rep)) == sizeof(rep));

    // the other task should exit before we have a chance to send
    ASSERT(send(misbehaving_receiving_tid, &msg, sizeof(msg), &rep, sizeof(rep)) == SEND_INCOMPLETE);
    printf("Misbehaving send done" EOL);
}

void misbehaving_receiving_task(void) {
    struct Msg msg;
    struct Reply rep;

    ASSERT(reply(-6, &rep, sizeof(rep)) == REPLY_IMPOSSIBLE_TID);
    ASSERT(reply(267, &rep, sizeof(rep)) == REPLY_IMPOSSIBLE_TID);
    ASSERT(reply(254, &rep, sizeof(rep)) == REPLY_INVALID_TID);
    ASSERT(reply(tid(), &rep, sizeof(rep)) == REPLY_INVALID_TID);

    // our parent should have exited by now, so we shouldn't be able to send to it
    ASSERT(reply(parent_tid(), &rep, sizeof(rep)) == REPLY_INVALID_TID);

    // we shouldn't be able to reply to somebody that hasn't sent a message to us
    ASSERT(reply(nop_tid, &rep, sizeof(rep)) == REPLY_UNSOLICITED);

    int tid;
    ASSERT(receive(&tid, &msg, sizeof(msg)) == sizeof(msg));
    ASSERT(reply(tid, &rep, sizeof(rep) + 1) == REPLY_TOO_LONG);
    ASSERT(reply(tid, &rep, sizeof(rep)) == REPLY_SUCCESSFUL);
    printf("Misbehaving receive done" EOL);
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

void message_suite(void) {
	receiving_tid = create(PRIORITY_MIN, receiving_task);
    for (int i = 0; i < producers; i++) {
        create(PRIORITY_MIN, sending_task);
    }

    misbehaving_receiving_tid = create(PRIORITY_MIN - 1, misbehaving_receiving_task);
    nop_tid = create(PRIORITY_MIN, nop);
    create(PRIORITY_MIN - 1, misbehaving_sending_task);
}

int main(int argc, char *argv[]) {
    boot(init_task, PRIORITY_MAX);
    boot(message_suite, PRIORITY_MAX);
}
