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
#ifdef QEMU
	ASSERT(fputs(COM1, "Changing train speed" EOL) == 0);
#else
	char train_command[2] = {speed, train};
	ASSERT(fput_buf(COM1, train_command, sizeof(train_command)) == 0);
#endif
}

static void tc_toggle_reverse(int train) {
	ASSERT(1 <= train && train <= 80);
#ifdef QEMU
	ASSERT(fputs(COM1, "Reversing train" EOL) == 0);
#else
	char train_command[2] = {15, train};
	ASSERT(fput_buf(COM1, train_command, sizeof(train_command)) == 0);
#endif
}

static void tc_stop(int train) {
	ASSERT(1 <= train && train <= 80);
#ifdef QEMU
	ASSERT(fputs(COM1, "Stopping train" EOL) == 0);
#else
	char train_command[2] = {0, train};
	ASSERT(fput_buf(COM1, train_command, sizeof(train_command)) == 0);
#endif
}

static void tc_switch_switch(int sw, enum sw_direction d) {
	ASSERT((1 <= sw && sw <= 18) || (145 <= sw && sw <= 148) || (150 <= sw && sw <= 156));
#ifdef QEMU
	ASSERT(fputs(COM1, "Changing switch position" EOL) == 0);
#else
	char cmd = (d == STRAIGHT) ? 0x21 : 0x22;
	char sw_command[2] = {cmd, sw};
	ASSERT(fput_buf(COM1, sw_command, sizeof(sw_command)) == 0);
#endif
}

static void tc_deactivate_switch(void) {
#ifdef QEMU
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
#define NUM_SPEED_SETTINGS 27
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
	// -1 iff there is no such train.
	// We don't allow multiple such trains on the track at once, since we wouldn't
	// be able to tell which train corresponds to which sensor hit. (The only way
	// we could do this would be to have them all going at different speeds, and
	// work out which train it was from the approx speed it should be going at, but
	// this is difficult, so we don't do this.)
	int unknown_train_id;

	struct switch_state switches;
	struct sensor_state sens_prev;
};

static inline struct internal_train_state* get_train_state(struct trainsrv_state *state, int train_id) {
	ASSERT(MIN_TRAIN <= train_id && train_id <= MAX_TRAIN);
	struct internal_train_state *train_state = state->state_for_train[train_id - 1];
	return train_state;
}

static struct internal_train_state* allocate_train_state(struct trainsrv_state *state, int train_id) {
	ASSERT(get_train_state(state, train_id) == NULL);
	ASSERT(state->num_active_trains + 1 < ARRAY_LENGTH(state->train_states));
	struct internal_train_state *train_state = &state->train_states[state->num_active_trains++];
	memset(train_state, 0, sizeof(*train_state));
	state->state_for_train[train_id - 1] = train_state;

	// initialize estimated speeds based on train id
	//int train_scaling_factor = 0;
	switch (train_id) {
	default:
		// TODO
		memset(train_state->est_velocities, 0, sizeof(*train_state->est_velocities));
		break;
	}
	// TODO: actually get constants from model, and calculate wild ass guess of
	// the train velocity table

	return train_state;
}

// TODO: write handlers for functions trying to inspect state of train, find
// how long it will take to travel a particular distance, or get the set of
// currently active trains

static void handle_set_speed(struct trainsrv_state *state, int train_id, int speed) {
	struct internal_train_state *train_state = get_train_state(state, train_id);
	if (train_state == NULL) {
		// we've never seen this train before
		ASSERT(state->unknown_train_id < 0);
		state->unknown_train_id = train_id;

		train_state = allocate_train_state(state, train_id);
	}
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
	start_reverse(train_id, train_state->current_speed_setting);
}
static void handle_sensors(struct trainsrv_state *state, struct sensor_state sens) {
	struct internal_train_state *train_state = NULL;
	if (state->unknown_train_id >= 0) {
		train_state = get_train_state(state, state->unknown_train_id);
	} else if (state->num_active_trains < 1) {
		return;
	} else {
		train_state = state->state_for_train[0];
	}
	// TODO: Currently this is horribly naive, and will only work when a
	// single train is on the track. It's also not robust against anything.
	int sensor = -1;
	void sens_handler(int sensor_) {
		ASSERT(sensor == -1);
		sensor = sensor_;
	}
	sensor_each_new(&state->sens_prev, &sens, sens_handler);
	train_state->last_known_position.edge = &track_node_from_sensor(sensor)->edge[0];
	train_state->last_known_position.displacement = 0;
	train_state->last_known_time = sens.ticks;
	state->sens_prev = sens;
}
static void handle_query_arrival(struct trainsrv_state *state, int train, int dist) {
	// TODO
}
static struct train_state handle_query_spatials(struct trainsrv_state *state, int train) {
	return (struct train_state){};
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
	calibrate_send_switches(whois(CALIBRATESRV_NAME), &state->switches);
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
