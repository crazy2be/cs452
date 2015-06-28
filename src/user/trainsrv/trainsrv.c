#include "../trainsrv.h"

#include "../clockserver.h"
#include "../nameserver.h"
#include "../request_type.h"
#include "../switch_state.h"
#include "../displaysrv.h"
#include "../calibrate/calibrate.h"
#include "../track.h"

#include "track_control.h"
#include "delayed_commands.h"
#include "estimate_position.h"

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

//
// train position estimation code
//

static void handle_sensors(struct trainsrv_state *state, struct sensor_state sens) {
	update_sensors(state, sens);
}

static void handle_set_speed(struct trainsrv_state *state, int train_id, int speed) {
	if (update_train_speed(state, train_id, speed)) {
		tc_set_speed(train_id, speed);
	}
}

static void handle_reverse(struct trainsrv_state *state, int train_id) {
	int current_speed = update_train_direction(state, train_id);
	start_reverse(train_id, current_speed);
}

static int handle_query_arrival(struct trainsrv_state *state, int train, int dist) {
	return (1000 * dist) / train_velocity(state, train);
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

static void handle_switch(struct trainsrv_state *state, int sw, enum sw_direction dir) {
	start_switch(sw, dir);
	update_switch(state, sw, dir);
}

static void trains_server(void) {
	register_as("trains");
	struct trainsrv_state state;

	trainsrv_state_init(&state);

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
			handle_switch(&state, req.switch_number, req.direction);
			// TODO: we need to reanchor the trains here
			/* displaysrv_update_switch(displaysrv, &switches); */
			reply(tid, NULL, 0);
			break;
		default:
			WTF("UNKNOWN TRAINS REQ %d"EOL, req.type);
			break;
		}
	}
}

void start_trains(void) {
	create(HIGHER(PRIORITY_MIN, 2), trains_server);
}

static int trains_tid(void) {
	static int ts_tid = -1;
	if (ts_tid < 0) {
		ts_tid = whois("trains");
	}
	return ts_tid;
}

static void trains_send(struct trains_request req, void *rpy, int rpyl) {
	send(trains_tid(), &req, sizeof(req), rpy, rpyl);
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
