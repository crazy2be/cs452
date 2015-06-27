#include "../trainsrv.h"

#include "../clockserver.h"
#include "../nameserver.h"
#include "../request_type.h"
#include "../switch_state.h"
#include "../displaysrv.h"
#include "../calibrate/calibrate.h"
#include "../track.h"

#include <util.h>
#include <kernel.h>
#include <assert.h>

struct trains_request {
	enum request_type type;

	int train_number;
	int speed;

	int switch_number;
	enum sw_direction direction; // TODO: union?

	int distance;
	struct sensor_state sensors;
};

#define MAX_TRAIN 80
#define MIN_TRAIN 1
#define NUM_TRAIN ((MAX_TRAIN) - (MIN_TRAIN) + 1)
static void tc_set_speed(int train, int speed) {
	ASSERT(1 <= train && train <= 80);
	ASSERT(0 <= speed && speed <= 14);
#ifdef ASCII_TC
	ASSERT(fputs(COM1, "Changing train speed" EOL) == 0);
#else
	char train_command[2] = {speed, train};
	ASSERT(fput_buf(COM1, train_command, sizeof(train_command)) == 0);
#endif
}

static void tc_toggle_reverse(int train) {
	ASSERT(1 <= train && train <= 80);
#ifdef ASCII_TC
	ASSERT(fputs(COM1, "Reversing train" EOL) == 0);
#else
	char train_command[2] = {15, train};
	ASSERT(fput_buf(COM1, train_command, sizeof(train_command)) == 0);
#endif
}

static void tc_stop(int train) {
	ASSERT(1 <= train && train <= 80);
#ifdef ASCII_TC
	ASSERT(fputs(COM1, "Stopping train" EOL) == 0);
#else
	char train_command[2] = {0, train};
	ASSERT(fput_buf(COM1, train_command, sizeof(train_command)) == 0);
#endif
}

static void tc_switch_switch(int sw, enum sw_direction d) {
	ASSERT((1 <= sw && sw <= 18) || (145 <= sw && sw <= 148) || (150 <= sw && sw <= 156));
#ifdef ASCII_TC
	ASSERT(fputs(COM1, "Changing switch position" EOL) == 0);
#else
	char cmd = (d == STRAIGHT) ? 0x21 : 0x22;
	char sw_command[2] = {cmd, sw};
	ASSERT(fput_buf(COM1, sw_command, sizeof(sw_command)) == 0);
#endif
}

static void tc_deactivate_switch(void) {
#ifdef ASCII_TC
	ASSERT(fputs(COM1, "Resetting solenoid" EOL) == 0);
#else
	ASSERT(fputc(COM1, 0x20) == 0);
#endif
}

struct reverse_info { int train; int speed; };
static void reverse_task(void) {
	int tid;
	struct reverse_info info;
	// our parent immediately sends us some bootstrap info
	receive(&tid, &info, sizeof(info));
	reply(tid, NULL, 0);

	tc_stop(info.train);
	delay(400);
	tc_toggle_reverse(info.train);
	tc_set_speed(info.train, info.speed);
}
static void start_reverse(int train, int speed) {
	int tid = create(HIGHER(PRIORITY_MIN, 2), reverse_task);
	struct reverse_info info = (struct reverse_info) {
		.train = train,
		.speed = speed,
	};
	send(tid, &info, sizeof(info), NULL, 0);
}

struct switch_info { int sw; enum sw_direction d; };
static void switch_task(void) {
	int tid;
	struct switch_info info;
	// our parent immediately sends us some bootstrap info
	receive(&tid, &info, sizeof(info));
	reply(tid, NULL, 0);

	tc_switch_switch(info.sw, info.d);
	delay(100);
	tc_deactivate_switch();
}
static void start_switch(int sw, enum sw_direction d) {
	int tid = create(HIGHER(PRIORITY_MIN, 2), switch_task);
	struct switch_info info = (struct switch_info) {
		.sw = sw,
		.d = d,
	};
	send(tid, &info, sizeof(info), NULL, 0);
}

//
// train position estimation code
//

// state about what we know about this train
// (previous_speed_was_bigger, current)
// (true, 0), (false, 1), (true, 1), ..., (false, 13), (true, 13), (false, 14)
#define NUM_SPEED_SETTINGS 28
struct internal_train_state {
	//  1. Its current estimated position
	//     position_unitialized(&position) iff train_id == state->unknown_train_id
	struct position last_known_position;
	int last_known_time;

	//  2. Our best guess for its velocity at each of the 27 speed settings
	//     (the speed setting behaves differently depending on if you accelerate or
	//     decelerate into that speed. You can only accelerate to speed 14, since
	//     its the highest. Therefore, 27 = 2 * 14 - 1.)
	int est_velocities[NUM_SPEED_SETTINGS];

	//  3. Its current speed setting
	int current_speed_setting;
	int previous_speed_setting;

	//  4. Later, we'll have information about if (and at what rate) it is currently
	//     accelerating
};

// catch-all struct to avoid static memory allocation for a user space task
struct trainsrv_state {
	struct internal_train_state train_states[MAX_ACTIVE_TRAINS];
	struct internal_train_state *state_for_train[NUM_TRAIN];
	int num_active_trains;

	// The id of a train that we've instructed to start moving, but we don't
	// know where it is on the track yet.
	// 0 iff there is no such train.
	// We don't allow multiple such trains on the track at once, since we wouldn't
	// be able to tell which train corresponds to which sensor hit. (The only way
	// we could do this would be to have them all going at different speeds, and
	// work out which train it was from the approx speed it should be going at, but
	// this is difficult, so we don't do this.)
	int unknown_train_id;

	struct switch_state switches;
	struct sensor_state sens_prev;
	int sensors_are_known;
};

static inline struct internal_train_state* get_train_state(struct trainsrv_state *state, int train_id) {
	ASSERT(MIN_TRAIN <= train_id && train_id <= MAX_TRAIN);
	struct internal_train_state *train_state = state->state_for_train[train_id - 1];
	return train_state;
}

static void initialize_train_velocity_table(struct internal_train_state *train_state, int train_id) {
	// our model is v = offset + speed_coef * speed + is_accelerated * is_accelerated_coef
	// basic linear model fitted in R
	// all of these are denominated in micrometers
	int offset, speed_coef, is_accelerated_coef;
	switch (train_id) {
	case 12:
		offset = -3816;
		speed_coef = 934;
		is_accelerated_coef = -605;
		break;
	case 62:
		offset = 1159;
		speed_coef = 357;
		is_accelerated_coef = -125;
		break;
	case 58:
		offset = -3404;
		speed_coef = 683;
		is_accelerated_coef = -315;
		break;
	default:
		memset(train_state->est_velocities, 0, sizeof(*train_state->est_velocities));
		return;
	}

	// calculate wild ass guess of the train velocity table
	for (int i = 0; i < NUM_SPEED_SETTINGS; i++) {
		int speed = (i + 1) / 2;
		int is_accelerated = (i + 1) % 2;
		train_state->est_velocities[i] = offset + speed * speed_coef + is_accelerated * is_accelerated_coef;
	}
}

static struct internal_train_state* allocate_train_state(struct trainsrv_state *state, int train_id) {
	ASSERT(get_train_state(state, train_id) == NULL);
	ASSERT(state->num_active_trains + 1 < ARRAY_LENGTH(state->train_states));
	struct internal_train_state *train_state = &state->train_states[state->num_active_trains++];
	memset(train_state, 0, sizeof(*train_state));
	state->state_for_train[train_id - 1] = train_state;

	// initialize estimated speeds based on train id
	//int train_scaling_factor = 0;
	initialize_train_velocity_table(train_state, train_id);

	return train_state;
}

// TODO: write handlers for functions trying to inspect state of train, find
// how long it will take to travel a particular distance, or get the set of
// currently active trains

static struct position get_estimated_train_position(struct trainsrv_state *state,
		struct internal_train_state *train_state);
static void reanchor(struct trainsrv_state *state,
					 struct internal_train_state *train_state) {
	train_state->last_known_position = get_estimated_train_position(state, train_state);
	train_state->last_known_time = time();
}
static void reanchor_all(struct trainsrv_state *state) {
	for (int i = 0; i < state->num_active_trains; i++) {
		reanchor(state, &state->train_states[i]);
	}
}
static void handle_set_speed(struct trainsrv_state *state, int train_id, int speed) {
	struct internal_train_state *train_state = get_train_state(state, train_id);
	if (train_state == NULL) {
		// we've never seen this train before
		ASSERT(state->unknown_train_id == 0);
		state->unknown_train_id = train_id;

		train_state = allocate_train_state(state, train_id);
	}
	// Just don't allow this so that we don't have to think about what the train
	// controller does in this case in terms of actual train speed.
	if (train_state->current_speed_setting == speed) return;
	reanchor(state, train_state); // TODO: Deacceration model.
	train_state->previous_speed_setting = train_state->current_speed_setting;
	train_state->current_speed_setting = speed;
	tc_set_speed(train_id, speed);
}
static bool position_unknown(struct trainsrv_state *state, int train_id) {
	ASSERT(train_id > 0);
	return state->unknown_train_id == train_id;
}
static void handle_reverse(struct trainsrv_state *state, int train_id) {
	struct internal_train_state *train_state = get_train_state(state, train_id);
	if ((train_state != NULL) && (!position_unknown(state, train_id))) {
		position_reverse(&train_state->last_known_position);
	}
	// TODO: This is basically wrong.
	// We want to reanchor whenever we actually change speed, which happens
	// twice when we are reversing.
	reanchor(state, train_state);
	start_reverse(train_id, train_state->current_speed_setting);
}

void sensor_cb(int sensor, void *ctx) {
	int *sensor_dest= (int*) ctx;
	*sensor_dest = sensor;
}

static void update_train_position_from_sensor(struct trainsrv_state *state,
		struct internal_train_state *train_state,
		int sensor, int ticks) {

	// if we had an estimated position for this train, ideally it should be estimated as
	// being very near to the tripped sensor when this happens
	/* struct train_state estimated_train_state; */
	/* int estimated_sensors_passed; */
 
	/* static void get_estimated_train_position(state, train_state, */
	/* 		&estimated_train_state, &estimated_sensors_passed); */

	// TODO: produce feedback about how much we were off by

	train_state->last_known_position.edge = &track_node_from_sensor(sensor)->edge[0];
	train_state->last_known_position.displacement = 0;
	train_state->last_known_time = ticks;
}

static void handle_sensors(struct trainsrv_state *state, struct sensor_state sens) {
	if (state->sensors_are_known) {
		struct internal_train_state *train_state = NULL;
		if (state->unknown_train_id > 0) {
			train_state = get_train_state(state, state->unknown_train_id);
		} else if (state->num_active_trains >= 1) {
			// TODO: Currently this is horribly naive, and will only work when a
			// single train is on the track. It's also not robust against anything.
			train_state = state->state_for_train[0];
		} else {
			return;
		}

		ASSERT(train_state != NULL);


		// TODO: this doesn't work if multiple sensors are tripped at the same time
		int sensor = -1;
		sensor_each_new(&state->sens_prev, &sens, sensor_cb, &sensor);
		if (sensor == -1) return;

		update_train_position_from_sensor(state, train_state, sensor, sens.ticks);
	} else {
		state->sensors_are_known = 1;
	}
	state->sens_prev = sens;
}

static int train_velocity_from_state(struct internal_train_state *train_state) {
	int cur_speed = train_state->current_speed_setting;
	int i = cur_speed*2 + (train_state->previous_speed_setting >= cur_speed) - 1;
	return train_state->est_velocities[i];
}

static int train_velocity(struct trainsrv_state *state, int train) {
	struct internal_train_state *train_state = get_train_state(state, train);
	ASSERT(train_state != NULL);
	return train_velocity_from_state(train_state);
}
static int handle_query_arrival(struct trainsrv_state *state, int train, int dist) {
	return (1000 * dist) / train_velocity(state, train);
}
struct bafc {
	const struct track_edge *last_edge;
	int dist_travelled;
};
// travel down the track for that many mm, and see which node we end at
bool break_after_distance(const struct track_edge *e, void *ctx) {
	struct position *pos = (struct position*)ctx;

	pos->edge = e;

	// Here, displacement is really the total distance travelled by the train in
	// the time interval since we last knew it's position
	// Note that this violates the usual invariant of displacement < edge->dist
	// We move to the next edge so long as our displacement is greater than the
	// length of the edge we're currently on.
	if (pos->displacement >= e->dist) {
		pos->displacement -= e->dist;
		return 0;
	} else {
		return 1;
	}
}

static struct position get_estimated_train_position(struct trainsrv_state *state,
		struct internal_train_state *train_state) {

	struct position position = train_state->last_known_position;

	if (position_is_uninitialized(&position)) {
		// the position is unknown, so just return unknown
		return position;
	}

	// find how much time has passed since the recorded position on the internal
	// train state, and how far we expect to have moved since then
	const int delta_t = time() - train_state->last_known_time;
	const int velocity = train_velocity_from_state(train_state);
	const struct track_edge *last_edge = train_state->last_known_position.edge;

	position.displacement += delta_t * velocity / 1000;

	// we special case the first edge like this since track_go_forwards takes a node, but we
	// start part of the way down an *edge*
	// If we didn't do this, if the starting node was a branch, we could be affected by the
	// direction of the switch even though we've passed the switch already
	if (position.displacement >= last_edge->dist) {
		position.displacement -= last_edge->dist;
		track_go_forwards(last_edge->dest, &state->switches, break_after_distance, &position);
	}

	return position;
}

static struct train_state handle_query_spatials(struct trainsrv_state *state, int train) {
	struct internal_train_state *train_state = get_train_state(state, train);
	ASSERT(train_state != NULL);

	return (struct train_state) {
		get_estimated_train_position(state, train_state),
		train_velocity_from_state(train_state),
	};
}

static void handle_query_active(struct trainsrv_state *state, int *trains) {
	memset(trains, 0, MAX_ACTIVE_TRAINS*sizeof(*trains));
	for (int i = 0; i < NUM_TRAIN; i++) {
		if (state->state_for_train[i] != NULL) {
			*trains = i + 1; //
		}
	}
}

static void trains_init(struct trainsrv_state *state) {
	memset(state, 0, sizeof(*state));
	for (int i = 1; i <= 18; i++) {
		tc_switch_switch(i, CURVED);
		switch_set(&state->switches, i, CURVED);
		// Avoid flooding rbuf. We probably shouldn't need this, since rbuf
		// should have plenty of space, but it seems to work...
		delay(1);
	}
	tc_switch_switch(153, CURVED);
	switch_set(&state->switches, 153, CURVED);
	tc_switch_switch(156, CURVED);
	switch_set(&state->switches, 156, CURVED);
	tc_deactivate_switch();
	displaysrv_update_switch(whois(DISPLAYSRV_NAME), &state->switches);
	//calibrate_send_switches(whois(CALIBRATESRV_NAME), &state->switches);
}

static void trains_server(void) {
	register_as("trains");
	struct trainsrv_state state;

	trains_init(&state);

	for (;;) {
		int tid = -1;
		struct trains_request req;
		receive(&tid, &req, sizeof(req));

		/* printf("Trains server got message! %d"EOL, req.type); */
		switch (req.type) {
		case QUERY_ACTIVE: {
			int active_trains[MAX_ACTIVE_TRAINS];
			handle_query_active(&state, active_trains);
			reply(tid, &active_trains, sizeof(active_trains));
			break;
		}
		case QUERY_SPATIALS: {
			struct train_state ts = handle_query_spatials(&state, req.train_number);
			reply(tid, &ts, sizeof(ts));
			break;
		}
		case QUERY_ARRIVAL:
			handle_query_arrival(&state, req.train_number, req.distance);
			reply(tid, NULL, 0);
			break;
		case SEND_SENSORS:
			handle_sensors(&state, req.sensors);
			reply(tid, NULL, 0);
			break;
		case SET_SPEED:
			// TODO: What do we do if we are already reversing or something?
			handle_set_speed(&state, req.train_number, req.speed);
			reply(tid, NULL, 0);
			break;
		case REVERSE:
			handle_reverse(&state, req.train_number);
			reply(tid, NULL, 0);
			break;
		case SWITCH_SWITCH:
			reanchor_all(&state);
			start_switch(req.switch_number, req.direction);
			switch_set(&state.switches, req.switch_number, req.direction);
			/* displaysrv_update_switch(displaysrv, &switches); */
			reply(tid, NULL, 0);
			break;
		default:
			WTF("UNKNOWN TRAINS REQ %d"EOL, req.type);
			break;
		}
	}
}

int start_trains(void) {
	return create(HIGHER(PRIORITY_MIN, 2), trains_server);
}

static int trains_tid(void) {
	static int ts_tid = -1;
	if (ts_tid < 0) {
		ts_tid = whois("trains");
	}
	return ts_tid;
}

static void trains_send(struct trains_request req, void *rpy, int rpyl) {
	ASSERTOK(send(trains_tid(), &req, sizeof(req), rpy, rpyl));
}
#define TSEND(req) trains_send(req, NULL, 0)
#define TSEND2(req, rpy) trains_send(req, rpy, sizeof(*(rpy)))

int trains_query_active(int *trains_out) {
	trains_send((struct trains_request) {
		.type = QUERY_ACTIVE,
	}, trains_out, sizeof(int)*MAX_ACTIVE_TRAINS);
	for (int i = 0; i < MAX_ACTIVE_TRAINS; i++) {
		if (trains_out[i] == 0) return i;
	}
	return MAX_ACTIVE_TRAINS;
}
void trains_query_spatials(int train, struct train_state *state_out) {
	TSEND2(((struct trains_request) {
		.type = QUERY_SPATIALS,
		.train_number = train,
	}), state_out);
}

int trains_query_arrival_time(int train, int distance) {
	int rpy = -1;
	TSEND2(((struct trains_request) {
		.type = QUERY_ARRIVAL,
		.train_number = train,
		.distance = distance,
	}), &rpy);
	return rpy;
}

void trains_send_sensors(struct sensor_state state) {
	TSEND(((struct trains_request) {
		.type = SEND_SENSORS,
		.sensors = state,
	}));
}

void trains_set_speed(int train, int speed) {
	TSEND(((struct trains_request) {
		.type = SET_SPEED,
		.train_number = train,
		.speed = speed,
	}));
}

void trains_reverse(int train) {
	TSEND(((struct trains_request) {
		.type = REVERSE,
		.train_number = train,
	}));
}

void trains_switch(int switch_number, enum sw_direction d) {
	TSEND(((struct trains_request) {
		.type = SWITCH_SWITCH,
		.switch_number = switch_number,
		.direction = d,
	}));
}
