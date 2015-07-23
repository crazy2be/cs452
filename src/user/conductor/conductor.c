#include "../conductor.h"

#include <kernel.h>
#include <util.h>
#include <assert.h>
#include "../trainsrv/track_data.h"
#include "../trainsrv/train_alert_srv.h"
#include "../trainsrv.h"
#include "../sys.h"
#include "../routesrv.h"

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
// A point of interest is a sensor number, and a displacement.
// We're assuming that the train is travelling only on the specified path.
// If the sensor is negative, this means that the point of interest is
// before the first sensor on our path, and should be triggered immediately.
// Clearly, this doesn't work for the stopping position (ie: short moves don't
// work)
struct point_of_interest {
	int sensor_num;
	int displacement;

	enum poi_type type;
	union {
		struct {
			int num;
			enum sw_direction dir;
		} switch_info;
	} u;
};

static void poi_from_node(struct astar_node *path, int index,
		struct point_of_interest *poi) {
	int displacement = 0;
	const struct track_node *node_ahead = path[index].node;
	while (--index >= 0) {
		const struct track_node *node = path[index].node;
		if (node->type == NODE_SENSOR) {
			poi->displacement = displacement;
			poi->sensor_num = node->num;
			return;
		} else {
			displacement += edge_between(node, node_ahead)->dist;
		}
	}
	poi->sensor_num = -1;
	poi->displacement = 0;
}

static struct point_of_interest get_next_poi(struct astar_node *path,
		int path_len, int *index_p, enum poi_type last_type,
		const struct position stopping_point) {

	int index;
	const struct track_node *node;
	struct point_of_interest poi;

	for (index = *index_p; index < path_len; index++) {
		node = path[index].node;
		if (last_type < SWITCH && node->type == NODE_BRANCH && index > 0 && index < path_len - 1) {
			poi.type = SWITCH;
			poi.u.switch_info.num = node->num;
			poi.u.switch_info.dir = (node->edge[0].dest == path[index + 1].node) ? STRAIGHT : CURVED;

			poi_from_node(path, index, &poi);
			break;
		} else if (last_type < STOPPING_POINT && node == stopping_point.edge->src) {
			poi_from_node(path, index, &poi);
			poi.displacement += stopping_point.displacement;
			poi.type = STOPPING_POINT;

			break;
		}
		last_type = NONE;
	}

	// TODO: this assert is failing
	ASSERT(index < path_len); // we should have broken out by now
	*index_p = index;

	return poi;
}

static struct position find_stopping_point(int train_id, struct astar_node *path, int path_len) {
	struct switch_state switches;
	memzero(&switches);
	const struct track_node *prev_node = path[0].node;
	int total_distance = 0;
	for (int i = 1; i < path_len; i++) {
		const struct track_node *cur_node = path[i].node;
		const struct track_edge *edge = edge_between(prev_node, cur_node);
		prev_node = cur_node;
		total_distance += edge->dist;
	}

	int stopping_distance = trains_get_stopping_distance(train_id);
	ASSERT(stopping_distance > 0);

	int distance_until_stop = total_distance - stopping_distance;
	// if this isn't asserted, we're trying to do a short stop, which we don't really support
	ASSERT(distance_until_stop > 0);

	prev_node = path[0].node;
	for (int i = 1; i < path_len; i++) {
		const struct track_node *cur_node = path[i].node;
		const struct track_edge *edge = edge_between(prev_node, cur_node);
		if (distance_until_stop < edge->dist) {
			struct position position = { edge, distance_until_stop };
			ASSERT(position_is_wellformed(&position));
			return position;
		}
		distance_until_stop -= edge->dist;
		prev_node = cur_node;
	}
	ASSERT(0);
	__builtin_unreachable();
}

struct conductor_state {
	int train_id;

	struct astar_node path[ASTAR_MAX_PATH];
	int path_len;
	int path_index;

	struct point_of_interest poi;
	int poi_index;

	// this is just cached
	struct position stopping_point;
};

// these are event driven
static void handle_stop_timeout(struct conductor_state *state) {
	trains_set_speed(state->train_id, 0);
}

static void handle_switch_timeout(int switch_num, enum sw_direction dir) {
	trains_switch(switch_num, dir);
}

static void handle_poi(struct conductor_state *state, int time) {
	int delay = trains_query_arrival_time(state->train_id, state->poi.displacement);

	struct conductor_req req;

	switch (state->poi.type) {
	case STOPPING_POINT:
		req.type = CND_STOP_TIMEOUT;
		break;
	case SWITCH:
		delay = MAX(delay - 20, 0);
		req.type = CND_SWITCH_TIMEOUT;
		req.u.switch_timeout.switch_num = state->poi.u.switch_info.num;
		req.u.switch_timeout.dir = state->poi.u.switch_info.dir;
		break;
	default:
		WTF("No such case");
		break;
	}

	int awake_at = time + delay;
	delay_async(awake_at, &req, sizeof(req), -1);

	state->poi = get_next_poi(state->path, state->path_len, &state->poi_index, state->poi.type, state->stopping_point);
}

static void handle_set_destination(const struct track_node *dest, struct conductor_state *state) {
	struct train_state train_state;
	trains_query_spatials(state->train_id, &train_state);

	state->path_len = routesrv_plan(train_state.position.edge->src, dest, state->path);

	printf("\e[s\e[14;90HWe're routing from %s to %s, found a path of length %d\e[u" EOL,
			train_state.position.edge->src->name, dest->name, state->path_len);

	if (state->path_len < 0) return; // no such path
	state->stopping_point = find_stopping_point(state->train_id, state->path, state->path_len);
	state->poi = get_next_poi(state->path, state->path_len, &state->poi_index, NONE, state->stopping_point);
	while (state->poi.sensor_num == -1) {
		ASSERT(state->poi.type == SWITCH);
		handle_switch_timeout(state->poi.u.switch_info.num, state->poi.u.switch_info.dir);
		state->poi = get_next_poi(state->path, state->path_len, &state->poi_index, state->poi.type, state->stopping_point);
	}
	trains_set_speed(state->train_id, 8);
}

static void handle_sensor_hit(int sensor_num, int time, struct conductor_state *state) {
	// check if we've gone off the projected path
	const unsigned error_tolerance = 2;
	unsigned i = 0;
	while (i < error_tolerance && i + state->path_index < state->path_len) {
		const struct track_node *node = state->path[state->path_index + i].node;
		if (node->type != NODE_SENSOR) continue;
		else if (node->num == sensor_num) break;
		else i++;
	}
	if (i >= error_tolerance) {
		// we've gone of the rails, so to speak
		state->path_len = 0; // go to idle mode
		// TODO: later, we should reroute when we error out like this
		return;
	}

	if (sensor_num == state->poi.sensor_num) handle_poi(state, time);
}

static void run_conductor(int train_id) {
	struct conductor_state state;
	memzero(&state);
	state.train_id = train_id;

	for (;;) {
		struct conductor_req req;
		int tid;
		receive(&tid, &req, sizeof(req));
		reply(tid, NULL, 0);

		printf("\e[s\e[13;90HConductor got request of type %d\e[u" EOL, req.type);

		// if the conductor is idling, discard events other than new dest events
		if (req.type != CND_DEST && state.path_len <= 0) continue;

		switch (req.type) {
		case CND_DEST:
			handle_set_destination(req.u.dest.dest, &state);
			break;
		case CND_SENSOR:
			handle_sensor_hit(req.u.sensor.sensor_num, req.u.sensor.time, &state);
			break;
		case CND_SWITCH_TIMEOUT:
			handle_stop_timeout(&state);
			break;
		case CND_STOP_TIMEOUT:
			handle_switch_timeout(req.u.switch_timeout.switch_num, req.u.switch_timeout.dir);
			break;
		default:
			WTF("Unknown request to conductor");
			break;
		}
	}
}

void conductor_get_name(int train_id, char (*buf)[CONDUCTOR_NAME_LEN]) {
	snprintf(*buf, CONDUCTOR_NAME_LEN, "conductor_%d", train_id);
}

struct conductor_params {
	int train_id;
};

static void init_conductor(void) {
	int tid;
	struct conductor_params params;
	receive(&tid, &params, sizeof(params));

	char conductor_name[CONDUCTOR_NAME_LEN];
	conductor_get_name(params.train_id, &conductor_name);

	// check that nobody else is registered
	ASSERT(-1 == try_whois(conductor_name));
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
