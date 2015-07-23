#include "tracksrv.h"

#include <kernel.h>
#include <assert.h>
#include "request_type.h"
#include "sys.h"
#include "signal.h"

struct tracksrv_request {
	enum request_type type;
	union {
		struct {
			int train_id;
		} set_train_id;
		struct {
			struct track_node *path;
			int len;
			int stopping_distance;
		} reserve_path;
	} u;
};

static int conductor_train_ids[256] = {}; // Just for debugging
static int reservation_table[TRACK_MAX] = {};

static int reserve_path(int tid, struct track_node *path,
						int len, int stopping_distance) {
	for (int i = 0; i < len; i++) {
		reservation_table[i] = tid;
	}
	return len;
}

void tracksrv(void) {
	register_as("tracksrv");
	signal_recv();
	for (;;) {
		struct tracksrv_request req = {};
		int tid = -1;
		recv(&tid, &req, sizeof(req));
		switch (req.type) {
		case TRK_RESERVE: {
			int res = reserve_path(tid,
								   req.u.reserve_path.path,
								   req.u.reserve_path.len,
								   req.u.reserve_path.stopping_distance);
			reply(tid, &res, sizeof(res));
			break;
		}
		case TRK_SET_ID: {
			int trid = req.u.set_train_id.train_id;
			ASSERT(conductor_train_ids[trid] == 0); // No changing trains?
			conductor_train_ids[trid] = tid;
			reply(tid, NULL, 0);
			break;
		}
		default:
			WTF();
		}
	}
}

void tracksrv_start(void) {
	int tid = create(PRIORITY_TRACKSRV, tracksrv);
	signal_send(tid);
}

int tracksrv_tid(void) {
	static int tid = -1;
	if (tid < 0) tid = whois("tracksrv");
	return tid;
}

void tracksrv_set_train_id(int train_id) {
	struct tracksrv_request req = (struct tracksrv_request) {
		.u.set_train_id.train_id = train_id,
	};
	send(tracksrv_tid(), &req, sizeof(req), NULL, 0);
}

int tracksrv_reserve_path(struct track_node *path, int len, int stopping_distance) {
	struct tracksrv_request req = (struct tracksrv_request) {
		.u.reserve_path.path = path,
		.u.reserve_path.len = len,
		.u.reserve_path.stopping_distance = stopping_distance,
	};
	int resp = -1;
	send(tracksrv_tid(), &req, sizeof(req), &resp, sizeof(resp));
	return resp;
}
