#include <kernel.h>
#include <assert.h>

#include <least_significant_set_bit.h>
#include <hashtable.h>
#include <prng.h>
#include <util.h>

#include "min_heap.h"
#include "track_test.h"

#include "../user/servers.h"
#include "../user/signal.h"
#include "../user/trainsrv.h"

void child(void) {
	printf("Child task %d" EOL, tid());
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

void memset_tests(void) {
	const unsigned bufsz = 80;
	unsigned char buf[bufsz];
	const char init = '\0';
	const char filler = '@';

	for (int lo = 0; lo < bufsz; lo++) {
		for (int hi = lo; hi < bufsz; hi++) {
			int len = hi - lo + 1;

			// naive memset to clear out the memory
			for (int i = 0; i < bufsz; i++) buf[i] = init;

			memset(buf + lo, filler, len);
			for (int i = 0; i < bufsz; i++) {
				const char expected = (lo <= i && i <= hi) ? filler : init;
				ASSERTF(buf[i] == expected, "buf[%d] = %d != %d", i, buf[i], expected);
			}
		}
	}
}

void sqrti_tests(void) {
	for (unsigned n = 0; n < 1000; n++) {
		unsigned m = sqrti(n);
		ASSERT(m * m <= n);
		ASSERT((m+1) * (m+1) > n);
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

		ASSERT(try_send(receiving_tid, &msg, sizeof(msg), &rep, sizeof(rep)) == sizeof(rep));
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

		ASSERT(try_receive(&tid, &msg, sizeof(msg)) == sizeof(msg));

		rep.sum = msg.a + msg.b;
		rep.prod = msg.a * msg.b;

		reply(tid, &rep, sizeof(rep));
	}
	printf("Receive done" EOL);
	signal_send(parent_tid());
}

void misbehaving_sending_task(void) {
	struct Msg msg;
	struct Reply rep;

	// check try_sending to impossible tasks
	ASSERT(try_send(-6, &msg, sizeof(msg), &rep, sizeof(rep)) == SEND_IMPOSSIBLE_TID);
	ASSERT(try_send(267, &msg, sizeof(msg), &rep, sizeof(rep)) == SEND_INVALID_TID);
	ASSERT(try_send(254, &msg, sizeof(msg), &rep, sizeof(rep)) == SEND_INVALID_TID);
	ASSERT(try_send(tid(), &msg, sizeof(msg), &rep, sizeof(rep)) == SEND_INVALID_TID);

	int child = create(PRIORITY_MAX, nop);
	// the child should exit immediately, so we shouldn't be able to try_send to it
	ASSERT(try_send(child, &msg, sizeof(msg), &rep, sizeof(rep)) == SEND_INVALID_TID);

	ASSERT(try_send(misbehaving_receiving_tid, &msg, sizeof(msg), &rep, sizeof(rep)) == sizeof(rep));

	// the other task should exit before we have a chance to try_send
	ASSERT(try_send(misbehaving_receiving_tid, &msg, sizeof(msg), &rep, sizeof(rep)) == SEND_INCOMPLETE);
	printf("Misbehaving try_send done" EOL);

	signal_send(parent_tid());
}

void misbehaving_receiving_task(void) {
	struct Msg msg;
	struct Reply rep;

	ASSERT(try_reply(-6, &rep, sizeof(rep)) == REPLY_IMPOSSIBLE_TID);
	ASSERT(try_reply(267, &rep, sizeof(rep)) == REPLY_INVALID_TID);
	ASSERT(try_reply(254, &rep, sizeof(rep)) == REPLY_INVALID_TID);
	ASSERT(try_reply(tid(), &rep, sizeof(rep)) == REPLY_INVALID_TID);

	int child = try_create(PRIORITY_MAX, nop);
	// the child should exit immediately, so we shouldn't be able to try_reply to it
	ASSERT(try_reply(child, &rep, sizeof(rep)) == REPLY_INVALID_TID);

	child = try_create(PRIORITY_MIN, nop);
	// we shouldn't be able to try_reply to somebody that hasn't sent a message to us
	// to do this test, we rely on the child not scheduling before us
	ASSERT(try_reply(child, &rep, sizeof(rep)) == REPLY_UNSOLICITED);

	int tid;
	ASSERT(try_receive(&tid, &msg, sizeof(msg)) == sizeof(msg));
	ASSERT(try_reply(tid, &rep, sizeof(rep) + 1) == REPLY_TOO_LONG);
	ASSERT(try_reply(tid, &rep, sizeof(rep)) == REPLY_SUCCESSFUL);
	printf("Misbehaving try_receive done" EOL);

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
		prng_gen_buf(&gen, buf, sizeof(buf));
		buf[sizeof(buf) - 1] = '\0';

		// assert that there are no collisions with our key generation
		ASSERT(HASHTABLE_KEY_NOT_FOUND == hashtable_get(&ht, buf, &val));

		ASSERT(HASHTABLE_SUCCESS == hashtable_set(&ht, buf, i));
	}

	prng_init(&gen, seed);
	for (i = 0; i < reps; i++) {
		prng_gen_buf(&gen, buf, sizeof(buf));
		buf[sizeof(buf) - 1] = '\0';

		ASSERT(HASHTABLE_SUCCESS == hashtable_get(&ht, buf, &val));
		ASSERT(i == val);
	}
}

void init_task(void) {
	start_servers();

	lssb_tests();
	hashtable_tests();
	memcpy_tests();
	memset_tests();
	sqrti_tests();
	min_heap_tests();
	track_tests();
	ASSERT(1);
	ASSERT(try_create(-1, child) == CREATE_INVALID_PRIORITY);
	ASSERT(try_create(32, child) == CREATE_INVALID_PRIORITY);

	while (try_create(PRIORITY_MIN, &nop) < 255);

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
	char buf[120];
	start_servers();
	fputs("Hello COM1" EOL, COM1);
	fprintf(COM1, "Hello COM1 9000 = %d, 0 = %d, -1 = %d or %u, 117 = %d, 0x7f = 0x%x, hello = %s" EOL,
			9000, 0, -1, -1, 117, 0x7f, "hello");
	ASSERT(87 == snprintf(buf, sizeof(buf), "Hello COM1 9 = %d, 0 = %d, -1 = %d or %u, 117 = %d, 0x7f = 0x%x, hello = %s" EOL,
		9, 0, -1, -1, 117, 0x7f, "hello"));
	fputs(buf, COM1);

	ASSERT(4 == snprintf(buf, sizeof(buf), "1234"));

	// make sure the overflow case is handled
	memset(buf, 'Q', 16);
	buf[16] = '\0';
	ASSERT(16 == snprintf(buf, 8, "123%d56%d890ABCDEF", 4, 7));
	ASSERT(strcmp(buf, "1234567") == 0);

	fputs("Hello COM2" EOL, COM2);
	stop_servers();
}

void destroy_worker(void) {
	int r, tid2;

	ASSERT(try_receive(&tid2, &r, sizeof(r)) == sizeof(r));
	reply(tid2, NULL, 0);

	ASSERT(try_receive(&tid2, NULL, 0) == 0);
	reply(tid2, &r, sizeof(r));
}

void destroy_init(void) {
	start_servers();
	int tids[256];
	int rands[256];
	int prev_n = -1;
	for (int round = 0; round < 10; round++) {
		int n = 0;
		// make tasks until we run out
		for (;;) {
			int tid = try_create(HIGHER(PRIORITY_MIN, 2), destroy_worker);
			ASSERT(tid >= 0 || tid == CREATE_INSUFFICIENT_RESOURCES);
			if (tid < 0) break;
			tids[n++] = tid;
		}
		// We should generate the same number of tasks every time.
		if (prev_n > 0) ASSERTF(prev_n == n, "%d %d", prev_n, n);
		// And always generate more than zero tasks.
		ASSERT(n > 0);
		prev_n = n;

		for (int i = 0; i < n; i++) {
			rands[i] = rand();
			ASSERT(try_send(tids[i], &rands[i], sizeof(rands[i]), NULL, 0) == 0);
		}

		for (int i = n - 1; i >= 0; i--) {
			int resp;
			ASSERT(try_send(tids[i], NULL, 0, &resp, sizeof(resp)) == 4);
			ASSERT(resp == rands[i]);
		}
	}
	printf("Done destroy stress test" EOL);
	stop_servers();
}

void trains_init(void) {
	start_servers();
	start_trains();
	trains_set_speed(14, 6);
	trains_reverse(14);
	printf("Done trains test" EOL);
	stop_servers();
}

int main(int argc, char *argv[]) {
	/* boot(trains_init, PRIORITY_MIN, 0); */
	/* return 0; */
	boot(init_task, PRIORITY_MIN, 0);
	boot(message_suite, PRIORITY_MIN, 0);
	boot(io_suite, PRIORITY_MIN, 0);
	boot(destroy_init, HIGHER(PRIORITY_MIN, 1), 0);
}
