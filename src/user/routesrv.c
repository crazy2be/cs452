#include "routesrv.h"

#include "signal.h"
#include "tracksrv.h"
#include "sys/nameserver.h"

struct route_request {
	const struct track_node *start;
	const struct track_node *end;
	struct astar_node *path_out;
	bool *blocked_table;
};

void routesrv(void) {
	register_as("route");
	signal_recv();
	for (;;) {
		int tid = -1;
		struct route_request req;
		recv(&tid, &req, sizeof(req));
		// TODO: this should be message passing the path back - this thing
		// is just writing memory which belongs to another task, which is ~illegal
		int res = astar_find_path(req.start, req.end, req.path_out, req.blocked_table);
		reply(tid, &res, sizeof(res));
	}
}

void routesrv_start(void) {
	int tid = create(PRIORITY_ROUTESRV, routesrv);
	signal_send(tid);
}

int routesrv_plan(const struct track_node *start, const struct track_node *end,
		struct astar_node *path_out) {
	static int route_tid = -1;
	if (route_tid < 0) route_tid = whois("route");

	int reservation_table[TRACK_MAX];
	tracksrv_get_reservation_table(reservation_table);
	int caller_tid = tid();
	bool blocked_table[TRACK_MAX] = {};
	for (int i = 0; i < TRACK_MAX; i++) {
		if ((reservation_table[i] > 0) && (reservation_table[i] != caller_tid)) {
			blocked_table[i] = true;
		}
	}

	struct route_request req = (struct route_request) {
		.start = start,
		.end = end,
		.path_out = path_out,
		.blocked_table = blocked_table,
	};
	int result = -2;
	send(route_tid, &req, sizeof(req), &result, sizeof(result));
	return result;
}
