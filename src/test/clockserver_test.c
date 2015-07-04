#include "clockserver_test.h"
#include "../user/sys.h"
#include <assert.h>

struct delay_msg { unsigned data; int ticks; };

void test_clockserver() {
	// mostly just smoke test
	start_servers();

	int t0, t1, t2;

	t0 = time();
	t1 = delay(1);
	printf("Hello 1" EOL);
	ASSERT(t1 == t0 + 1);

	t2 = delay_until(t1 + 1);
	printf("Hello 2" EOL);
	ASSERT(t2 == t1 + 1);

	struct delay_msg msg = { 0xdeadbeef, 0 };
	delay_async(1, &msg, sizeof(msg), offsetof(struct delay_msg, ticks));
	printf("Hello 3" EOL);

	int tid;
	struct delay_msg msg2;
	receive(&tid, &msg2, sizeof(msg2));
	reply(tid, NULL, 0);
	ASSERT(msg2.data == msg.data);
	ASSERT(msg2.ticks == t2 + 1);

	stop_servers();
}
