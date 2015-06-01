#include "nameserver.h"

// TODO: these deps should not live in the kernel dir
#include "../kernel/hashtable.h"
#include "../kernel/util.h"
#include "request_type.h"

#include <kernel.h>

struct request {
	enum request_type type;
	int ticks;
};

void clocknotifier(void) {
	struct request req;
	req.type = TICK_HAPPENED;
	for (;;) {
		await(EID_TIMER_TICK);
		send(CLOCKSERVER_TID, &req, sizeof(req), NULL, 0);
	}
}
void clockserver(void) {
	struct min_heap delied;
	min_heap_init(&delied);

	int num_ticks = 0;
	create(PRIORITY_MAX, &clocknotifier);

	for (;;) {
		int tid, resp, err;
		struct request req;
		receive(&tid, &req, sizeof(req));

		switch (req.type) {
		case TICK_HAPPENED:
			reply(tid, NULL, 0);
			num_ticks++;
			if (min_heap_top_key(&delied) >= num_ticks) {
				int awoken_tid = min_heap_pop(&delied);
				reply(awoken_tid, &resp, sizeof(resp));
			}
			break;
		case DELAY:
			min_heap_push(&delied, now() + req.ticks, tid);
			break;
		case TIME:
			resp = num_ticks;
			reply(tid, &resp, sizeof(resp));
			break;
		case DELAY_UNTIL:
			min_heap_push(&delied, req.ticks, tid);
			break;
		default:
			resp = -1;
			reply(tid, &resp, sizeof(resp));
			break;
		}
	}
}

int delay(int ticks) {
	int rpy;
	struct request req = (struct request) {
		.type = DELAY,
		.ticks = ticks,
	};
	int l = send(CLOCKSERVER_TID, &req, sizeof(req), &rpy, sizeof(rpy));
	if (l != sizeof(rpy)) return l;
	return rpy;
}

int time() {
	int rpy;
	struct request req = (struct request) {
		.type = TIME,
	};
	int l = send(CLOCKSERVER_TID, &req, sizeof(req), &rpy, sizeof(rpy));
	if (l != sizeof(rpy)) return l;
	return rpy;
}

int delay_until(int ticks) {
	int rpy;
	struct request req = (struct request) {
		.type = DELAY_UNTIL,
		.ticks = ticks,
	};
	int l = send(CLOCKSERVER_TID, &req, sizeof(req), &rpy, sizeof(rpy));
	if (l != sizeof(rpy)) return l;
	return rpy;
}
