#include <kernel.h>
#include <assert.h>

#include <least_significant_set_bit.h>
#include <hashtable.h>
#include <prng.h>
#include <util.h>

#include "min_heap.h"
#include "../user/servers.h"
#include "../user/signal.h"

void child(void) {
	printf(COM2, "Child task %d" EOL, tid());
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
	signal_send(parent_tid());
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
	printf(COM2, "Receive done" EOL);
	signal_send(parent_tid());
}

void misbehaving_sending_task(void) {
	struct Msg msg;
	struct Reply rep;

	// check sending to impossible tasks
	ASSERT(send(-6, &msg, sizeof(msg), &rep, sizeof(rep)) == SEND_IMPOSSIBLE_TID);
	ASSERT(send(267, &msg, sizeof(msg), &rep, sizeof(rep)) == SEND_IMPOSSIBLE_TID);
	ASSERT(send(254, &msg, sizeof(msg), &rep, sizeof(rep)) == SEND_INVALID_TID);
	ASSERT(send(tid(), &msg, sizeof(msg), &rep, sizeof(rep)) == SEND_INVALID_TID);

	int child = create(PRIORITY_MAX, nop);
	// the child should exit immediately, so we shouldn't be able to send to it
	ASSERT(send(child, &msg, sizeof(msg), &rep, sizeof(rep)) == SEND_INVALID_TID);

	ASSERT(send(misbehaving_receiving_tid, &msg, sizeof(msg), &rep, sizeof(rep)) == sizeof(rep));

	// the other task should exit before we have a chance to send
	ASSERT(send(misbehaving_receiving_tid, &msg, sizeof(msg), &rep, sizeof(rep)) == SEND_INCOMPLETE);
	printf(COM2, "Misbehaving send done" EOL);

	signal_send(parent_tid());
}

void misbehaving_receiving_task(void) {
	struct Msg msg;
	struct Reply rep;

	ASSERT(reply(-6, &rep, sizeof(rep)) == REPLY_IMPOSSIBLE_TID);
	ASSERT(reply(267, &rep, sizeof(rep)) == REPLY_IMPOSSIBLE_TID);
	ASSERT(reply(254, &rep, sizeof(rep)) == REPLY_INVALID_TID);
	ASSERT(reply(tid(), &rep, sizeof(rep)) == REPLY_INVALID_TID);

	int child = create(PRIORITY_MAX, nop);
	// the child should exit immediately, so we shouldn't be able to reply to it
	ASSERT(reply(child, &rep, sizeof(rep)) == REPLY_INVALID_TID);

	child = create(PRIORITY_MIN, nop);
	// we shouldn't be able to reply to somebody that hasn't sent a message to us
	// to do this test, we rely on the child not scheduling before us
	ASSERT(reply(child, &rep, sizeof(rep)) == REPLY_UNSOLICITED);

	int tid;
	ASSERT(receive(&tid, &msg, sizeof(msg)) == sizeof(msg));
	ASSERT(reply(tid, &rep, sizeof(rep) + 1) == REPLY_TOO_LONG);
	ASSERT(reply(tid, &rep, sizeof(rep)) == REPLY_SUCCESSFUL);
	printf(COM2, "Misbehaving receive done" EOL);

	signal_send(parent_tid());
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
	start_servers();

	lssb_tests();
	hashtable_tests();
	memcpy_tests();
	min_heap_tests();
	ASSERT(1);
	ASSERT(create(-1, child) == CREATE_INVALID_PRIORITY);
	ASSERT(create(32, child) == CREATE_INVALID_PRIORITY);

	while (create(PRIORITY_MIN, &nop) < 255);

	ASSERT(create(4, child) == CREATE_INSUFFICIENT_RESOURCES);

	stop_servers();
}

void message_suite(void) {
	start_servers();

	receiving_tid = create(PRIORITY_MIN, receiving_task);
	for (int i = 0; i < producers; i++) {
		create(PRIORITY_MIN, sending_task);
	}
	misbehaving_receiving_tid = create(PRIORITY_MIN - 1, misbehaving_receiving_task);
	create(PRIORITY_MIN - 1, misbehaving_sending_task);

	for (int i = 0; i < producers + 3; i++) signal_recv();

	stop_servers();
}

void io_suite(void) {
	start_servers();
	puts(COM1, "Hello COM1" EOL);
	printf(COM1, "Hello COM1 9 = %d, 0 = %d, -1 = %d or %u, 117 = %d, 0x7f = 0x%x" EOL,
			9, 0, -1, -1, 117, 0x7f);
	puts(COM2, "Hello COM2" EOL);
	stop_servers();
}

int main(int argc, char *argv[]) {
	boot(init_task, PRIORITY_MIN, 0);
	boot(message_suite, PRIORITY_MIN, 0);
	boot(io_suite, PRIORITY_MIN, 0);
}
