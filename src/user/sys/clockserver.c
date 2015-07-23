#include "clockserver.h"
#include "nameserver.h"
#include "../request_type.h"

#include <kernel.h>
#include <assert.h>

#define ASYNC_MSG_BUFSZ 80
struct queued_async_request {
	int tid;
	unsigned buf_len;
	unsigned buf_tick_offset;
	unsigned char buf[ASYNC_MSG_BUFSZ];
};

#define MIN_HEAP_VALUE struct queued_async_request
#define MIN_HEAP_PREFIX async_req
#include <min_heap.h>

#undef MIN_HEAP_VALUE
#undef MIN_HEAP_PREFIX
#define MIN_HEAP_VALUE int
#define MIN_HEAP_PREFIX sync_req
#include <min_heap.h>

struct clockserver_request {
	enum request_type type;
	int ticks;

	// only used for async requests
	unsigned buf_len;
	unsigned buf_tick_offset;
	unsigned char buf[ASYNC_MSG_BUFSZ];
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

static void courier(void) {
	struct queued_async_request qreq;
	int tid;
	receive(&tid, &qreq, sizeof(qreq));
	reply(tid, NULL, 0);
	send(qreq.tid, qreq.buf, qreq.buf_len, NULL, 0);
}

void clockserver(void) {
	register_as("clockserver");

	struct sync_req_min_heap sync_delayed;
	sync_req_min_heap_init(&sync_delayed);

	struct async_req_min_heap async_delayed;
	async_req_min_heap_init(&async_delayed);

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
			while (!sync_req_min_heap_empty(&sync_delayed) && sync_req_min_heap_top_key(&sync_delayed) <= num_ticks) {
				int awoken_tid = sync_req_min_heap_pop(&sync_delayed);
				reply(awoken_tid, &num_ticks, sizeof(num_ticks));
			}
			while (!async_req_min_heap_empty(&async_delayed) && async_req_min_heap_top_key(&async_delayed) <= num_ticks) {
				struct queued_async_request qreq = async_req_min_heap_pop(&async_delayed);
				int courier_tid = create(LOWER(PRIORITY_MAX, 1), courier);
				int *tick_p = (int*)(qreq.buf + qreq.buf_tick_offset);
				*tick_p = num_ticks;
				send(courier_tid, &qreq, sizeof(qreq), NULL, 0);
			}
			break;
		case DELAY:
			ASSERTF(req.ticks >= 0, "%d", req.ticks);
			sync_req_min_heap_push(&sync_delayed, num_ticks + req.ticks, tid);
			break;
		case DELAY_UNTIL:
			ASSERTF(req.ticks >= 0, "%d", req.ticks);
			sync_req_min_heap_push(&sync_delayed, req.ticks, tid);
			break;
		case DELAY_ASYNC: {
			struct queued_async_request qreq;
			qreq.tid = tid;
			qreq.buf_len = req.buf_len;
			qreq.buf_tick_offset = req.buf_tick_offset;
			memcpy(qreq.buf, req.buf, req.buf_len);
			async_req_min_heap_push(&async_delayed, num_ticks + req.ticks, qreq);
			reply(tid, NULL, 0);
			break;
		}
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

void delay_async(int ticks, void *msg, unsigned msg_len, unsigned msg_tick_offset) {
	struct clockserver_request req;
	ASSERT(msg_len <= sizeof(req.buf));

	req.type = DELAY_ASYNC;
	req.ticks = ticks;
	req.buf_len = msg_len;
	req.buf_tick_offset = msg_tick_offset;
	memcpy(req.buf, msg, msg_len);

	send(clockserver_tid(), &req, sizeof(req), NULL, 0);
}
