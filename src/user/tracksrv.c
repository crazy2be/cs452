#include "tracksrv.h"

#include <kernel.h>
#include <assert.h>
#include "request_type.h"
#include "sys.h"
#include "signal.h"
#include "displaysrv.h"
#define idx(node) ({ \
	int ix = TRACK_NODE_INDEX(node); \
	ASSERTF(ix >= 0 && ix < TRACK_MAX, "%d", ix); \
	ix; })

struct tracksrv_request {
	enum request_type type;
	union {
		struct {
			int train_id;
		} set_train_id;
		struct {
			struct astar_node *path;
			int len;
			int stopping_distance;
			int tid;
		} reserve_path;
		struct {
			int *table_out;
		} table;
	} u;
};

static int conductor_train_ids[256] = {}; // Just for debugging
static int reservation_table[TRACK_MAX] = {};

static int edge_num_btwn(const struct track_node *a, const struct track_node *b) {
	if (a->edge[0].dest == b) return 0;
	else if (a->edge[1].dest == b) return 1;
	else {WTF("Discontinuous path! %p %p %p %p", a, b, a->edge[0].dest, a->edge[1].dest); return -1;}
}
static void desire_i(int *desired, const struct track_node *node) {
	desired[idx(node)] = 1;
	desired[idx(node->reverse)] = 1;
}
static void desire(int *desired, const struct track_node *node) {
	desire_i(desired, node);
	if (node->num == 153) desire_i(desired, find_track_node("BR154"));
	if (node->num == 154) desire_i(desired, find_track_node("BR153"));
	if (node->num == 155) desire_i(desired, find_track_node("BR156"));
	if (node->num == 156) desire_i(desired, find_track_node("BR155"));
}
#define edge_btwn(a, b) (&a->edge[edge_num_btwn(a, b)])
static void reserve_forwards(int tid, int *desired, const struct track_node *start, int dist) {
	int iterations = 0;
	const struct track_node *prev = NULL;
	const struct track_node *cur = start;
	int rem_dist = dist;
	for (;;) {
		desire(desired, cur);
		if (rem_dist <= 0) return;
		else if (cur->type == NODE_EXIT) return;
		else if (iterations++ > TRACK_MAX) {WTF("Cycle");}
		else if (cur->type == NODE_BRANCH) {
			// Just mark all directions. Technically this is more than we need
			// to do, but it allows us to remain ignorant of switches.
			reserve_forwards(tid, desired, cur->edge[0].dest,
							 rem_dist - edge_btwn(cur, cur->edge[0].dest)->dist);
			reserve_forwards(tid, desired, cur->edge[1].dest,
							 rem_dist - edge_btwn(cur, cur->edge[1].dest)->dist);
			return;
		} else {
			if (prev) {
				rem_dist -= edge_btwn(prev, cur)->dist;
			}
			prev = cur;
			cur = cur->edge[0].dest;
		}
	}
}
static int reserve_path(int tid, struct astar_node *path,
						int len, int stopping_distance) {
	ASSERT(len >= 0);
	for (int i = 0; i < len; i++) {
		idx(path[i].node); // Validate
	}
	int total_path_length = 0;
	for (int i = 0; i < len - 1; i++) {
		total_path_length += edge_btwn(path[i].node, path[i+1].node)->dist;
	}
	int desired[TRACK_MAX] = {};
	int remaining_path_length = total_path_length;
	const struct track_node *prev = NULL;
	for (int i = 0; i < len; i++) {
		const struct track_node *cur = path[i].node;
		desire(desired, cur);
		if (prev) remaining_path_length -= edge_btwn(prev, cur)->dist;
		if (prev && (cur->type == NODE_BRANCH) && (i < len - 1)) {
			int rem_dist = MIN(stopping_distance, remaining_path_length);
			int edge_num = edge_num_btwn(prev, cur);
			int other_edge_num = (edge_num + 1) % 2;
			const struct track_node *start = cur->edge[other_edge_num].dest;
			rem_dist -= edge_btwn(cur, start)->dist;
			reserve_forwards(tid, desired, start, rem_dist);
		}
		prev = cur;
	}
	int unreservable = 0;
	for (int i = 0; i < TRACK_MAX; i++) {
		if (desired[i] && reservation_table[i] && (reservation_table[i] != tid)) {
			unreservable++;
		}
	}
	if (unreservable > 0) return -unreservable;

	int reserved = 0;
	for (int i = 0; i < TRACK_MAX; i++) {
		if (reservation_table[i] == tid) {
			reservation_table[i] = 0;
		}
		if (desired[i]) {
			reservation_table[i] = tid;
			reserved++;
		}
	}
	return reserved;
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
			int res = reserve_path(req.u.reserve_path.tid,
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
		case TRK_TABLE: {
			memcpy(req.u.table.table_out, reservation_table, sizeof(reservation_table));
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
		.type = TRK_SET_ID,
		.u.set_train_id.train_id = train_id,
	};
	send(tracksrv_tid(), &req, sizeof(req), NULL, 0);
}

int tracksrv_reserve_path_test(struct astar_node *path, int len, int stopping_distance, int tid) {
	struct tracksrv_request req = (struct tracksrv_request) {
		.type = TRK_RESERVE,
		.u.reserve_path.path = path,
		.u.reserve_path.len = len,
		.u.reserve_path.stopping_distance = stopping_distance,
		.u.reserve_path.tid = tid,
	};
	int resp = -1;
	send(tracksrv_tid(), &req, sizeof(req), &resp, sizeof(resp));
	return resp;
}

int tracksrv_reserve_path(struct astar_node *path, int len, int stopping_distance) {
	int n = tracksrv_reserve_path_test(path, len, stopping_distance, tid());
	int track_table[TRACK_MAX];
	tracksrv_get_reservation_table(track_table);
	displaysrv_update_track_table(whois("displaysrv"), track_table);
	return n;
}

void tracksrv_get_reservation_table(int *table_out) {
	struct tracksrv_request req = (struct tracksrv_request) {
		.type = TRK_TABLE,
		.u.table.table_out = table_out,
	};
	send(tracksrv_tid(), &req, sizeof(req), NULL, 0);
}
