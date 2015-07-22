#include "../conductor.h"

#include <kernel.h>
#include <util.h>
#include <assert.h>
#include "../trainsrv/track_data.h"
#include "../trainsrv/train_alert_srv.h"
#include "../trainsrv.h"
#include "../sys.h"
#include "../routesrv.h"

struct conductor_params {
	int train_id;
};

static int get_route(int train_id, struct astar_node (*path)[ASTAR_MAX_PATH]) {
	// in our idle state, we wait for a command telling us a new destination
	const struct track_node *destination;

	int tid;
	receive(&tid, &destination, sizeof(destination));
	reply(tid, NULL, 0);

	struct train_state state;
	trains_query_spatials(train_id, &state);

	return routesrv_plan(state.position.edge->src, destination, path);
}

static const struct track_edge *edge_between(const struct track_node *prev, const struct track_node *cur) {
	ASSERT(prev->type != NODE_EXIT); // this wouldn't make any sense
	const struct track_edge *edge = &prev->edge[DIR_STRAIGHT];
	if (prev->type == NODE_BRANCH && edge->dest != cur) {
		edge = &prev->edge[DIR_CURVED];
	}
	// this must be true, or the route planner is fucked
	// (TODO: this will need to get slightly more clever to allow reversing)
	ASSERT(edge->dest == cur);
	return edge;
}

enum poi_type { NONE, SWITCH, STOPPING_POINT };
struct point_of_interest {
	struct position position;
	enum poi_type type;
	union {
		struct {
			int num;
			enum sw_direction dir;
		} switch_info;
	} u;
};

static struct point_of_interest get_next_poi(struct astar_node (*path)[ASTAR_MAX_PATH],
		int path_len, int *index_p, enum poi_type last_type,
		const struct position stopping_point) {

	int index;
	const struct track_node *node;
	struct point_of_interest poi;

	for (index = *index_p; index < path_len; index++) {
		node = (*path)[index].node;
		if (last_type < SWITCH && node->type == NODE_BRANCH && index != 0) {
			// travel backwards through the path for a distance, to give us time for the
			// turnout to switch
			int switch_lead_distance = 200; // mm
			const struct track_node *current_node = node;
			const struct track_edge *edge = NULL;
			for (int i = index - 1; switch_lead_distance > 0 && i >= 0; i--) {
				const struct track_node *prev_node = (*path)[i].node;
				edge = edge_between(prev_node, current_node);
				switch_lead_distance -= edge->dist;
				current_node = prev_node;
			}
			ASSERT(edge != NULL);
			poi.position.edge = edge;
			poi.position.displacement = (switch_lead_distance > 0) ? 0 : -switch_lead_distance;
			poi.type = SWITCH;
			poi.u.switch_info.num = node->num;
			poi.u.switch_info.dir = ((*path)[index - 1].node->edge[0].dest == node) ? STRAIGHT : CURVED;
			break;
		} else if (last_type < STOPPING_POINT && node == stopping_point.edge->src) {
			poi.position = stopping_point;
			poi.type = STOPPING_POINT;
		}
		last_type = NONE;
	}

	ASSERT(index < path_len); // we should have broken out by now
	*index_p = index;

	return poi;
}

static struct position find_stopping_point(int train_id, struct astar_node (*path)[ASTAR_MAX_PATH], int path_len) {
	struct switch_state switches;
	memzero(&switches);
	const struct track_node *prev_node = (*path)[0].node;
	int total_distance = 0;
	for (int i = 1; i < path_len; i++) {
		const struct track_node *cur_node = (*path)[i].node;
		const struct track_edge *edge = edge_between(prev_node, cur_node);
		prev_node = cur_node;
		total_distance += edge->dist;
	}

	int stopping_distance = trains_get_stopping_distance(train_id);

	int distance_until_stop = total_distance - stopping_distance;
	// if this isn't asserted, we're trying to do a short stop, which we don't really support
	ASSERT(distance_until_stop > 0);

	prev_node = (*path)[0].node;
	for (int i = 1; i < path_len; i++) {
		const struct track_node *cur_node = (*path)[i].node;
		const struct track_edge *edge = edge_between(prev_node, cur_node);
		if (distance_until_stop < edge->dist) {
			return (struct position) { edge, distance_until_stop };
		}
		distance_until_stop -= edge->dist;
		prev_node = cur_node;
	}
	ASSERT(0);
	__builtin_unreachable();
}

static void run_conductor(int train_id) {
	for (;;) {
		struct astar_node path[ASTAR_MAX_PATH];
		int path_len = get_route(train_id, &path);

		ASSERT(path_len > 0);

		struct train_state state;
		trains_query_spatials(train_id, &state);
		if (path[0].node != state.position.edge->src) {
			// then we need to start from the reverse
			if (state.speed_setting == 0) {
				trains_reverse_unsafe(train_id);
			} else {
				trains_reverse(train_id);
				delay(500);
			}
		}

		delay(10);
		trains_set_speed(train_id, 14);

		struct position stopping_point = find_stopping_point(train_id, &path, path_len);

		int index = 0;
		enum poi_type last_type = NONE;
		while (index < path_len) {
			// we don't reserve any track right now, since we assume we're the only
			// train on the track

			// we compute the next POI on the track:
			// right now, the only POI on the track are
			//  1) the position we want to stop at
			//  2) a little bit in advance of a switch on our path, so
			//     we can ensure that the switch is in the right position
			struct point_of_interest next_poi = get_next_poi(&path, path_len, &index, last_type, stopping_point);

			train_alert_at(train_id, next_poi.position);
			switch (next_poi.type) {
			case STOPPING_POINT:
				trains_set_speed(train_id, 0);
				break;
			case SWITCH:
				trains_switch(next_poi.u.switch_info.num, next_poi.u.switch_info.dir);
				break;
			default:
				WTF("No such case");
				break;
			}
			last_type = next_poi.type;
		}
	}
}

static void init_conductor(void) {
	int tid;
	struct conductor_params params;
	receive(&tid, &params, sizeof(params));

	char conductor_name[20];
	snprintf(conductor_name, sizeof(conductor_name), "conductor_%d", params.train_id);

	// check that nobody else is registered
	ASSERT(-1 == whois(conductor_name));
	register_as(conductor_name);

	reply(tid, NULL, 0);

	run_conductor(params.train_id);
}

int conductor(int train_id) {
	struct conductor_params params = { train_id };
	int tid = create(PRIORITY_MIN, init_conductor);
	send(tid, &params, sizeof(params), NULL, 0);
	return tid;
}
