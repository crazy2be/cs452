#include "../conductor.h"

#include <kernel.h>
#include <util.h>
#include <assert.h>
#include "../trainsrv/track_data.h"
#include "../trainsrv/train_alert_srv.h"
#include "../trainsrv.h"
#include "../sys.h"
#include "../routesrv.h"
#include "conductor_internal.h"
#include "../displaysrv.h"

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

static const char *poi_type_desc(enum poi_type pt) {
	static const char *type_desc[NUM_POI_TYPE] = {
		"none", "switch", "stopping_point"};
	ASSERT(pt >= 0 && pt < NUM_POI_TYPE);
	return type_desc[pt];
}

static void poi_from_node(struct astar_node *path, int path_len, int index, struct point_of_interest *poi,
		int lead_dist) {
	ASSERT(index >= 0 && index < path_len);
	poi->original = path[index].node;

	int displacement = lead_dist;
	while (index >= 0) {
		const struct track_node *node = NULL;
		int sensor_gap = 0;
		index--;
		while (index >= 0) {
			ASSERT(index >= 0 && index + 1 < path_len);
			node = path[index].node;
			sensor_gap += edge_between(node, path[index + 1].node)->dist;
			if (node->type == NODE_SENSOR) break;
			index--;
		}

		if (index < 0) {
			break;
		} else if (sensor_gap >= displacement) {
			poi->sensor_num = node->num;
			poi->displacement = sensor_gap - displacement;
			poi->path_index = index;
			ASSERT(poi->delay >= 0);
			return;
		} else {
			displacement -= sensor_gap;
		}
	}
	poi->sensor_num = -1;
	poi->displacement = 0;
	poi->path_index = 0;
}

static bool poi_lte(struct point_of_interest *a, struct point_of_interest *b) {
	// reject NONE if there's an alternative
	if (a->type == NONE || b->type == NONE) {
		return b->type == NONE;
	} else if (a->path_index == b->path_index) {
		// TODO: This should really compare delays (although it seems like
		// comparing displacements may be equivelent?)
		return a->displacement < b->displacement;
	} else {
		return a->path_index < b->path_index;
	}
}

static inline bool poi_context_finished(struct poi_context pc, int path_len) {
	return pc.poi_index >= path_len && pc.stopped;
}

struct point_of_interest get_next_poi(struct astar_node *path, int path_len,
		struct poi_context *context, int stopping_distance, int velocity) {

	int index = -1;
	struct point_of_interest switch_poi = {};
	struct point_of_interest stopping_poi = {};

	// initialize poi for stopping point
	if (!context->stopped) {
		//logf("Stopping at %s, distance: %d", path[path_len - 1].node->name, stopping_distance);
		poi_from_node(path, path_len, path_len - 1, &stopping_poi, stopping_distance);
		stopping_poi.type = STOPPING_POINT;
	}

	// next, check for switch poi that come before it, overwriting if
	for (index = context->poi_index; index + 1 < path_len; index++) {
		ASSERT(index >= 0 && index + 1 < path_len);
		const struct track_node *node = path[index].node;
		if (node->type != NODE_BRANCH) continue;

		if (index == 0) {
			switch_poi.sensor_num = -1;
			switch_poi.displacement = 0;
		} else {
			// This is probably way more distance in the future than we need
			// to reserve, but me may as well be conservative.
			poi_from_node(path, path_len, index, &switch_poi, 500);
		}

		switch_poi.type = SWITCH;
		switch_poi.u.switch_info.num = node->num;
		switch_poi.u.switch_info.dir =
			(node->edge[0].dest == path[index + 1].node) ? STRAIGHT : CURVED;

		break;
	}

	if (switch_poi.type == NONE) {
		ASSERT(index >= path_len - 1);
		context->poi_index = path_len;
	}

	struct point_of_interest *chosen_poi = &stopping_poi;
	struct point_of_interest *candidates[] = { &switch_poi };

	for (int i = 0; i < ARRAY_LENGTH(candidates); i++) {
		if (poi_lte(candidates[i], chosen_poi)) {
			chosen_poi = candidates[i];
		}
	}

	// chose which poi we want to use, and then set the state so we won't repeat it
	if (chosen_poi == &stopping_poi) {
		ASSERT(!context->stopped);
		context->stopped = true;
	} else if (chosen_poi == &switch_poi) {
		context->poi_index = index + 1;
		//logf("Switch poi chosen (stopping: %d)", stopping_poi.displacement);
	}

	if (chosen_poi->type == NONE) {
		logf("Next POI: [None]");
	} else {
		// add delay to chosen_poi
		// TODO: this should account for acceleration, and will get more complex later
		chosen_poi->delay = chosen_poi->displacement * 1000 / velocity;

		char repr[4] = "N/A";
		if (chosen_poi->sensor_num > 0) {
			sensor_repr(chosen_poi->sensor_num, repr);
		}
		const char *targ = "[NULL]";
		if (chosen_poi->original != NULL) {
			targ = chosen_poi->original->name;
		}
		logf("Next POI [%d]: %s + %d mm / %d ticks, type %s, targ %s",
				context->poi_index,
				repr, chosen_poi->displacement, chosen_poi->delay,
				poi_type_desc(chosen_poi->type), targ);
	}

	return *chosen_poi;
}

// these are event driven
static void handle_stop_timeout(struct conductor_state *state) {
	logf("Handling stop timeout");
	trains_set_speed(state->train_id, 0);
}

static void handle_switch_timeout(int switch_num, enum sw_direction dir) {
	logf("Handling switch timeout");
	trains_switch(switch_num, dir);
}

static void handle_poi(struct conductor_state *state, int _) {
	struct conductor_req req = {};

	/* int offset; */
	switch (state->poi.type) {
	case STOPPING_POINT:
		req.type = CND_STOP_TIMEOUT;
		req.u.stop_timeout.expected_time = time() + state->poi.delay;
		/* offset = offsetof(struct conductor_req, u.stop_timeout.time); */
		break;
	case SWITCH:
		req.type = CND_SWITCH_TIMEOUT;
		req.u.switch_timeout.switch_num = state->poi.u.switch_info.num;
		req.u.switch_timeout.dir = state->poi.u.switch_info.dir;
		req.u.switch_timeout.expected_time = time() + state->poi.delay;
		/* offset = offsetof(struct conductor_req, u.switch_timeout.time); */
		break;
	default:
		WTF("No such case");
	}
	logf("Delaying %d ticks until poi of type %s",
		 state->poi.delay,
		 poi_type_desc(state->poi.type));
	delay_async(state->poi.delay, &req, sizeof(req), -1);

	// prevent the poi from being handled twice
	state->poi.type = NONE;
}

const int max_speed = 14;

static void handle_set_destination(const struct track_node *dest, struct conductor_state *state) {
	struct train_state train_state = {};
	trains_query_spatials(state->train_id, &train_state);

	state->path_len = routesrv_plan(train_state.position.edge->src, dest, state->path);
	state->path_index = 0; // drop the first node in the route, since we've already rolled over it
	state->poi_context.poi_index = 0;
	state->poi_context.stopped = false;

	logf("We're routing from %s to %s, found a path of length %d",
			train_state.position.edge->dest->name, dest->name, state->path_len);

	if (state->path_len <= 0) return; // no such path
	// NOTE: this is a bit of a hack - we really just want to check for poi whose sensor we've already passed over
	// we don't know the train's speed yet, so we just fudge it with a value that shouldn't matter anyway
	// (the velocity is only used if we need to delay a long time ahead of the switch, but if we're at a dead
	// stop, we don't)
	trains_set_speed(state->train_id, max_speed);
	trains_query_spatials(state->train_id, &train_state);

	int velocity = train_state.velocity;
	state->last_velocity = velocity;

	int stopping_distance = trains_get_stopping_distance(state->train_id);
	logf("Calculating inital pois...");
	state->poi = get_next_poi(state->path, state->path_len, &state->poi_context, stopping_distance, velocity);
	ASSERT(state->poi.type != NONE);
	ASSERT(state->poi.type != STOPPING_POINT);

	// Fire off any events that are before the first sensor on our route.
	while (state->poi.type != NONE && state->poi.sensor_num == -1) {
		ASSERT(state->poi.type != STOPPING_POINT);
		handle_switch_timeout(state->poi.u.switch_info.num, state->poi.u.switch_info.dir);
		state->poi = get_next_poi(state->path, state->path_len, &state->poi_context, stopping_distance, velocity);
	}

	logf("Waiting to hit first sensor...");
}

static void set_next_poi(int time, struct conductor_state *state) {
	struct train_state train_state = {};
	trains_query_spatials(state->train_id, &train_state);
	int velocity = train_state.velocity;
	if (velocity == 0) {
		velocity = state->last_velocity;
	} else {
		state->last_velocity = velocity;
	}

	int stopping_distance = trains_get_stopping_distance(state->train_id);
	int last_sensor = state->poi.sensor_num;
	state->poi = get_next_poi(state->path, state->path_len, &state->poi_context,
			stopping_distance, velocity);
	if (state->poi.type != NONE) {
		if (state->poi.sensor_num == last_sensor) {
			// account for time passed we hit the last sensor
			state->poi.delay -= time - state->last_sensor_time;
			if (state->poi.delay < 0) {
				logf("We passed (%d ticks late) this POI already, begin event now",
					state->poi.delay);
				state->poi.delay = 0;
			}
			handle_poi(state, time);
		} else if (state->poi.path_index < state->path_index) {
			logf("We passed [path_index] (%d ticks late) this POI already, begin event now",
				state->poi.delay);
			// fuck - we've passed this already - just kick off the event immediately
			state->poi.delay = 0;
			handle_poi(state, time);
		}
	}
}

static void handle_sensor_hit(int sensor_num, int time, struct conductor_state *state) {
	// check if we've gone off the projected path
	const unsigned error_tolerance = 2;
	unsigned errors = 0;
	int i = -1;
	for (i = state->path_index; errors < error_tolerance && i < state->path_len; i++) {
		const struct track_node *node = state->path[i].node;
		if (node->type != NODE_SENSOR) continue;
		char repr[4];
		sensor_repr(node->num, repr);
// 		logf("Checking hit against sensor %s, index = %d", repr, i);
		if (node->num == sensor_num) break;
		else errors++;
	}
	char repr[4];
	sensor_repr(sensor_num, repr);
	if (errors >= error_tolerance) {
		// we've gone of the rails, so to speak
		state->poi.type = NONE; // go to idle mode
		trains_set_speed(state->train_id, 0);
		logf("We've diverged from our desired path at sensor %s", repr);
		// TODO: later, we should reroute when we error out like this
		return;
	} else if (i >= state->path_len) {
		return;
	} else if (state->poi.type != NONE && i >= state->poi.path_index) {
		logf("Approaching poi at sensor %s", repr);
		handle_poi(state, time);
		state->last_sensor_time = time;
	} else {
		logf("On track at sensor %s", repr);
	}

	state->path_index = i + 1;
}

static void run_conductor(int train_id) {
	struct conductor_state state = {};
	state.train_id = train_id;

	for (;;) {
		struct conductor_req req = {};
		int tid = -1;
		receive(&tid, &req, sizeof(req));
		reply(tid, NULL, 0);

		// if the conductor is idling, discard events other than new dest events
		if (req.type == CND_SENSOR && state.poi.type == NONE) {
			//logf("Conductor got %d request, but ignored it", req.type);
			continue;
		}

		switch (req.type) {
		case CND_DEST:
			logf("Conductor got dest request");
			handle_set_destination(req.u.dest.dest, &state);
			break;
		case CND_SENSOR:
			//logf("Conductor got sensor request");
			handle_sensor_hit(req.u.sensor.sensor_num, req.u.sensor.time, &state);
			break;
		case CND_SWITCH_TIMEOUT: {
			//logf("Conductor got switch timeout request");
			int now = time();
			if (req.u.switch_timeout.expected_time != now) {
				logf("Got switch timeout at %d, expected at %d", now,
					 req.u.switch_timeout.expected_time);
			}
			handle_switch_timeout(req.u.switch_timeout.switch_num, req.u.switch_timeout.dir);
			set_next_poi(time(), &state);
			break;
		}
		case CND_STOP_TIMEOUT: {
			logf("Conductor got stop timeout request");
			int now = time();
			if (req.u.stop_timeout.expected_time != now) {
				logf("Got stop timeout at %d, expected at %d", now,
					 req.u.stop_timeout.expected_time);
			}
			handle_stop_timeout(&state);
			set_next_poi(time(), &state);
			break;
		}
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
	int tid = -1;
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
