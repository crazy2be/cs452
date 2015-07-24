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
	int delay;

	int path_index; // internal use only, for ease of comparing poi

	enum poi_type type;
	union {
		struct {
			int num;
			enum sw_direction dir;
		} switch_info;
	} u;
};

static void poi_from_node(struct astar_node *path, int index, struct point_of_interest *poi,
		int velocity, int lead_dist, int lead_time) {
	ASSERT(velocity > 0);
	int displacement = lead_dist + velocity * lead_time / 1000;
	while (index >= 0) {
		const struct track_node *node;
		int sensor_gap = 0;
		while (--index >= 0) {
			node = path[index].node;
			sensor_gap += edge_between(node, path[index + 1].node)->dist;
			if (node->type == NODE_SENSOR) break;
		}

		if (index < 0) {
			break;
		} else if (sensor_gap >= displacement) {
			poi->sensor_num = node->num;
			poi->delay = (sensor_gap - displacement) * 1000 / velocity;
			poi->path_index = index;
			ASSERT(poi->delay >= 0);
			return;
		} else {
			displacement -= sensor_gap;
		}
	}
	poi->sensor_num = -1;
}

static bool poi_lte(struct point_of_interest *a, struct point_of_interest *b) {
	if (a->type == NONE || b->type == NONE) { // reject NONE if there's an alternative
		return b->type == NONE;
	} else if (a->path_index == b->path_index) {
		return a->delay < b->delay;
	} else {
		return a->path_index < b->path_index;
	}
}

struct poi_context {
	bool stopped;
	int poi_index; // for switches
};

static inline bool poi_context_finished(struct poi_context pc, int path_len) {
	return pc.poi_index >= path_len && pc.stopped;
}

static struct point_of_interest get_next_poi(struct astar_node *path, int path_len,
		struct poi_context *context, int stopping_distance, int velocity) {

	printf("Called POI" EOL);

	int index;
	const struct track_node *node = NULL;
	struct point_of_interest poi;
	poi.type = NONE;

	struct point_of_interest stopping_point;
	stopping_point.type = NONE;

	// initialize to poi for stopping point

	if (!context->stopped) {
		poi_from_node(path, path_len - 1, &stopping_point, velocity, stopping_distance, 0);
		stopping_point.type = STOPPING_POINT;
	}

	// next, check for switch poi that come before it, overwriting if
	for (index = context->poi_index; index < path_len; index++) {
		node = path[index].node;
		if (node->type == NODE_BRANCH && index > 0 && index < path_len - 1) {
			poi_from_node(path, index, &poi, velocity, 0, 40);

			poi.type = SWITCH;
			poi.u.switch_info.num = node->num;
			poi.u.switch_info.dir = (node->edge[0].dest == path[index + 1].node) ? STRAIGHT : CURVED;

			break;
		}
	}

	// chose which poi we want to use, and then set the state so we won't repeat it
	if (poi_lte(&stopping_point, &poi)) {
		poi = stopping_point;
		context->stopped = true;
	} else {
		context->poi_index = index;
	}

	ASSERT(poi.type != NONE || !poi_context_finished(*context, path_len));
	char repr[4];
	sensor_repr(poi.sensor_num, repr);
	printf("\e[s\e[15;90HNext POI is sensor %s + %d ticks, real target is %s\e[u", repr, poi.delay, (node != NULL) ? node->name : "(NULL)");
	return poi;
}

struct conductor_state {
	int train_id;

	struct astar_node path[ASTAR_MAX_PATH];
	int path_len;
	int path_index;

	struct point_of_interest poi;
	struct poi_context poi_context;
};

// these are event driven
static void handle_stop_timeout(struct conductor_state *state) {
	printf("\e[s\e[19;90HHandling stop timeout\e[u");
	trains_set_speed(state->train_id, 0);
}

static void handle_switch_timeout(int switch_num, enum sw_direction dir) {
	printf("\e[s\e[18;90HHandling switch timeout\e[u");
	trains_switch(switch_num, dir);
}

static void handle_poi(struct conductor_state *state, int time) {
	struct conductor_req req;

	char *kind;

	switch (state->poi.type) {
	case STOPPING_POINT:
		kind = "stopping point";
		req.type = CND_STOP_TIMEOUT;
		break;
	case SWITCH:
		kind = "switch";
		req.type = CND_STOP_TIMEOUT;
		req.type = CND_SWITCH_TIMEOUT;
		req.u.switch_timeout.switch_num = state->poi.u.switch_info.num;
		req.u.switch_timeout.dir = state->poi.u.switch_info.dir;
		break;
	default:
		WTF("No such case");
		return;
	}

	int delay = state->poi.delay;
	char repr[4];
	sensor_repr(state->poi.sensor_num, repr);
	printf("\e[s\e[17;90HWe're approaching POI %s (%s), delay_async for %d\e[u",
			repr, kind, delay);

	delay_async(delay, &req, sizeof(req), -1);
}

const int max_speed = 14;

static void handle_set_destination(const struct track_node *dest, struct conductor_state *state) {
	struct train_state train_state;
	trains_query_spatials(state->train_id, &train_state);

	state->path_len = routesrv_plan(train_state.position.edge->src, dest, state->path);
	state->path_index = 1; // drop the first node in the route, since we've already rolled over it
	state->poi_context.poi_index = 1;
	state->poi_context.stopped = false;

	if (state->path_len <= 0) return; // no such path
	// NOTE: this is a bit of a hack - we really just want to check for poi whose sensor we've already passed over
	// we don't know the train's speed yet, so we just fudge it with a value that shouldn't matter anyway
	// (the velocity is only used if we need to delay a long time ahead of the switch, but if we're at a dead
	// stop, we don't)
	trains_set_speed(state->train_id, max_speed);
	trains_query_spatials(state->train_id, &train_state);

	int velocity = train_state.velocity;
	int stopping_distance = trains_get_stopping_distance(state->train_id);
	state->poi = get_next_poi(state->path, state->path_len, &state->poi_context, stopping_distance, velocity);
	ASSERT(state->poi.type != NONE);
	ASSERT(state->poi.type != STOPPING_POINT);
	printf("\e[s\e[14;90HWe're routing from %s to %s, found a path of length %d, first poi is for sensor %d + %d ticks\e[u",
			train_state.position.edge->src->name, dest->name, state->path_len, state->poi.sensor_num, state->poi.delay);

	while (state->poi.type != NONE &&
			(state->poi.sensor_num == -1 ||
			(state->path[0].node->type == NODE_SENSOR && state->poi.sensor_num == state->path[0].node->num))) {
		ASSERT(state->poi.type != STOPPING_POINT);
		handle_switch_timeout(state->poi.u.switch_info.num, state->poi.u.switch_info.dir);
		state->poi = get_next_poi(state->path, state->path_len, &state->poi_context, stopping_distance, velocity);
	}

	printf("\e[s\e[14;90HWe're routing from %s to %s, found a path of length %d\e[u",
			train_state.position.edge->src->name, dest->name, state->path_len);
}

static void handle_sensor_hit(int sensor_num, int time, struct conductor_state *state) {
	// check if we've gone off the projected path
	const unsigned error_tolerance = 2;
	unsigned errors = 0;
	int i;
	for (i = state->path_index; errors < error_tolerance && i < state->path_len; i++) {
		const struct track_node *node = state->path[i].node;
		if (node->type != NODE_SENSOR) continue;
		else if (node->num == sensor_num) break;
		else errors++;
	}
	char repr[4];
	sensor_repr(sensor_num, repr);
	if (errors >= error_tolerance) {
		// we've gone of the rails, so to speak
		state->poi.type = NONE; // go to idle mode
		trains_set_speed(state->train_id, 0);
		printf("\e[s\e[16;90HWe've diverged from our desired path at sensor %s\e[u", repr);
		// TODO: later, we should reroute when we error out like this
		return;
	}

	if (state->poi.type == NONE) {
		return;
	} else if (sensor_num == state->poi.sensor_num) {
		printf("\e[s\e[16;90HApproaching poi at sensor %s\e[u", repr);
		struct train_state train_state;
		trains_query_spatials(state->train_id, &train_state);
		int stopping_distance = trains_get_stopping_distance(state->train_id);
		while (state->poi.type != NONE && state->poi.sensor_num == sensor_num) {
			handle_poi(state, time);
			state->poi = get_next_poi(state->path, state->path_len, &state->poi_context,
					stopping_distance, train_state.velocity);
		}
	} else {
		printf("\e[s\e[16;90HOn track at sensor %s\e[u", repr);
	}

	state->path_index = MAX(state->path_len, i + 1);
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

		// if the conductor is idling, discard events other than new dest events
		if (req.type == CND_SENSOR && state.poi.type == NONE) {
			printf("\e[s\e[13;90HConductor got %d request, but ignored it\e[u", req.type);
			continue;
		}

		switch (req.type) {
		case CND_DEST:
			printf("\e[s\e[13;90HConductor got dest request\e[u");
			handle_set_destination(req.u.dest.dest, &state);
			break;
		case CND_SENSOR:
			printf("\e[s\e[13;90HConductor got sensor request\e[u");
			handle_sensor_hit(req.u.sensor.sensor_num, req.u.sensor.time, &state);
			break;
		case CND_SWITCH_TIMEOUT:
			printf("\e[s\e[13;90HConductor got switch timeout request\e[u");
			handle_switch_timeout(req.u.switch_timeout.switch_num, req.u.switch_timeout.dir);
			break;
		case CND_STOP_TIMEOUT:
			printf("\e[s\e[13;90HConductor got stop timeout request\e[u");
			handle_stop_timeout(&state);
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
	int tid = create(PRIORITY_CONDUCTOR, init_conductor);
	send(tid, &params, sizeof(params), NULL, 0);
	return tid;
}
