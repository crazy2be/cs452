#include "clockserver.h"
#include "nameserver.h"
#include "../request_type.h"

#include <min_heap.h>
#include <kernel.h>
#include <assert.h>

struct clockserver_request {
	enum request_type type;
	int ticks;
};

static void clocknotifier(void) {
	struct clockserver_request req;
	int clockserver_tid = parent_tid();
	req.type = TICK_HAPPENED;
	for (;;) {
		int ticks = await(EID_TIMER_TICK, NULL, 0);
		ASSERT(ticks >= 0);
		req.ticks = ticks;
		ASSERT(send(clockserver_tid, &req, sizeof(req), NULL, 0) == 0);
	}
}

void clockserver(void) {
	struct min_heap delayed;
	min_heap_init(&delayed);
	register_as("clockserver");

	int num_ticks = 0;
	create(PRIORITY_MAX, &clocknotifier);

	for (;;) {
		int tid, resp;
		struct clockserver_request req;
		receive(&tid, &req, sizeof(req));

		switch (req.type) {
		case TICK_HAPPENED:
			reply(tid, NULL, 0);
			// we shouldn't be skipping any ticks
			// a weaker form of this assertion would be to check that time never goes backwards
			if (num_ticks + 1 != req.ticks) {
				printf("num_ticks = %d, req.ticks = %d" EOL, num_ticks, req.ticks);
			}
			//ASSERT(num_ticks + 1 == req.ticks);
			num_ticks = req.ticks;
			while (!min_heap_empty(&delayed) && min_heap_top_key(&delayed) <= num_ticks) {
				int awoken_tid = min_heap_pop(&delayed);
				reply(awoken_tid, &num_ticks, sizeof(num_ticks));
			}
			break;
		case DELAY:
			//printf("Clockserver got delay %d"EOL, req.ticks);
			ASSERTF(req.ticks >= 0, "%d", req.ticks);
			min_heap_push(&delayed, num_ticks + req.ticks, tid);
			break;
		case DELAY_UNTIL:
			ASSERTF(req.ticks >= 0, "%d", req.ticks);
			min_heap_push(&delayed, req.ticks, tid);
			break;
		case TIME:
			resp = num_ticks;
			reply(tid, &resp, sizeof(resp));
			break;
		default:
			resp = -1;
			printf("UNKNOWN REQ" EOL);
			reply(tid, &resp, sizeof(resp));
			break;
		}
	}
}


static int clockserver_tid(void) {
	static int cs_tid = -1;
	if (cs_tid < 0) cs_tid = whois("clockserver");
	return cs_tid;
}
static int csend(struct clockserver_request req) {
	int rpy = -1;
	send(clockserver_tid(), &req, sizeof(req), &rpy, sizeof(rpy));
	return rpy;
}
int delay(int ticks) {
	return csend((struct clockserver_request) {
		.type = DELAY,
		 .ticks = ticks,
	});
}
int time() {
	return csend((struct clockserver_request) {
		.type = TIME,
	});
}
int delay_until(int ticks) {
	return csend((struct clockserver_request) {
		.type = DELAY_UNTIL,
		 .ticks = ticks,
	});
}
