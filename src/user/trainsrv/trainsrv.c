#include "../trainsrv.h"

#include "../sys.h"
#include "../displaysrv.h"
#include "../track.h"

#include "track_control.h"
#include "delayed_commands.h"
#include "estimate_position.h"
#include "train_alert_srv.h"
#include "trainsrv_request.h"
#include "speed_history.h"

#include <util.h>
#include <kernel.h>
#include <assert.h>

//
// train position estimation code
//

static void handle_sensors(struct trainsrv_state *state, struct sensor_state sens) {
	update_sensors(state, sens);
}

static void handle_set_speed(struct trainsrv_state *state, int train_id, int speed) {
	if (update_train_speed(state, train_id, speed)) {
		// TODO: we should get rid of this behaviour, as it causes more problems than it solves
		tc_set_speed(train_id, speed);
	}
}

static void handle_reverse(struct trainsrv_state *state, int train_id) {
	struct internal_train_state *train_state = get_train_state(state, train_id);
	ASSERT(train_state != NULL);
	int current_speed = speed_historical_get_current(&train_state->speed_history);
	start_reverse(train_id, current_speed);
}
static void handle_reverse_unsafe(struct trainsrv_state *state, int train_id) {
	struct internal_train_state *train_state = get_train_state(state, train_id);
	ASSERT(train_state != NULL);
	int current_speed = speed_historical_get_current(&train_state->speed_history);
	ASSERT(current_speed == 0);
	train_state->reversed = true;

	tc_toggle_reverse(train_id);
	// TODO: update train state to point in opposite direction
}

static int handle_query_arrival(struct trainsrv_state *state, int train, int dist) {
	return train_eta(state, train, dist);
}

static struct train_state handle_query_spatials(struct trainsrv_state *state, int train) {
	struct internal_train_state *train_state = get_train_state(state, train);
	struct train_state out;
	if (train_state == NULL) {
		memset(&out, 0, sizeof(out));
	} else {
		out.position = get_estimated_train_position(state, train_state);
		out.velocity = train_velocity_from_state(train_state);
		out.speed_setting = speed_historical_get_current(&train_state->speed_history);
	}
	return out;
}

static int handle_query_error(struct trainsrv_state *state, int train) {
	struct internal_train_state *train_state = get_train_state(state, train);
	if (train_state == NULL) {
		return 0;
	}
	return train_state->measurement_error;
}

static int handle_query_active(struct trainsrv_state *state, int * const trains) {
	int *p = trains;
	for (int i = 0; i < NUM_TRAIN; i++) {
		if (state->state_for_train[i] != NULL) {
			*p++ = i + 1; //
		}
	}
	const int len = p - trains;
	ASSERT_INTEQ(len, state->num_active_trains);
	return len;
}

static void ensure_uncurved(struct trainsrv_state *state, int sw) {
	struct switch_state switches = \
		switch_historical_get_current(&state->switch_history);
	if (switch_get(&switches, sw) == CURVED) {
		tc_switch_switch(sw, STRAIGHT);
		tc_deactivate_switch();
		update_switch(state, sw, STRAIGHT);
	}
}
static void handle_switch(struct trainsrv_state *state, int sw, enum sw_direction dir) {
	tc_switch_switch(sw, dir);
	tc_deactivate_switch();
	// On the middle switches, curved curved == BAD BAD. Prevent this
	// configuration by automatically rewriting it to curved straight, which
	// is probably what you want.
	if (sw == 153 && dir == CURVED) ensure_uncurved(state, 154);
	if (sw == 154 && dir == CURVED) ensure_uncurved(state, 153);
	if (sw == 155 && dir == CURVED) ensure_uncurved(state, 156);
	if (sw == 156 && dir == CURVED) ensure_uncurved(state, 155);
	update_switch(state, sw, dir);
}

static void trains_server(void) {
	register_as("trains");
	struct trainsrv_state state;

	trainsrv_state_init(&state);

	train_alert_start(switch_historical_get_current(&state.switch_history), true);

	// TODO: we should block the creating task from continuing until init is done

	for (;;) {
		int tid = -1;
		struct trains_request req;
		receive(&tid, &req, sizeof(req));

		/* printf("Trains server got message! %d"EOL, req.type); */
		switch (req.type) {
		case QUERY_ACTIVE: {
			int active_trains[MAX_ACTIVE_TRAINS];
			int num_active_trains = handle_query_active(&state, active_trains);
			reply(tid, &active_trains, num_active_trains * sizeof(active_trains[0]));
			break;
		}
		case QUERY_SPATIALS: {
			struct train_state ts = handle_query_spatials(&state, req.train_number);
			reply(tid, &ts, sizeof(ts));
			break;
		}
		case QUERY_ARRIVAL: {
			int ticks = handle_query_arrival(&state, req.train_number, req.distance);
			reply(tid, &ticks, sizeof(ticks));
			break;
		}
		case QUERY_ERROR: {
			int error = handle_query_error(&state, req.train_number);
			reply(tid, &error, sizeof(error));
			break;
		}
		case SEND_SENSORS:
			handle_sensors(&state, req.sensors);
			reply(tid, NULL, 0);
			break;
		case SET_SPEED:
			handle_set_speed(&state, req.train_number, req.speed);
			reply(tid, NULL, 0);
			break;
		case REVERSE:
			handle_reverse(&state, req.train_number);
			reply(tid, NULL, 0);
			break;
		case REVERSE_UNSAFE:
			handle_reverse_unsafe(&state, req.train_number);
			reply(tid, NULL, 0);
			break;
		case SWITCH_SWITCH:
			handle_switch(&state, req.switch_number, req.direction);
			reply(tid, NULL, 0);
			break;
		case SWITCH_GET: {
			struct switch_state switches = switch_historical_get_current(&state.switch_history);
			reply(tid, &switches, sizeof(switches));
			break;
		}
		case GET_STOPPING_DISTANCE: {
			struct internal_train_state *ts = get_train_state(&state, req.train_number);
			int distance = ts->est_stopping_distances[train_speed_index(ts, 1)];
			reply(tid, &distance, sizeof(distance));
			break;
		}
		case SET_STOPPING_DISTANCE: {
			struct internal_train_state *ts = get_train_state(&state, req.train_number);
			ts->est_stopping_distances[train_speed_index(ts, 1)] = req.stopping_distance;
			reply(tid, NULL, 0);
			break;
		}
		case GET_LAST_KNOWN_SENSOR: {
			struct internal_train_state *ts = get_train_state(&state, req.train_number);
			int last_sensor_hit = sensor_historical_get_current(&ts->sensor_history);
			reply(tid, &last_sensor_hit, sizeof(last_sensor_hit));
			break;
		}
		default:
			nameserver_dump_names();
			WTF("UNKNOWN TRAINS REQ %d FROM %d"EOL, req.type, tid);
			break;
		}
	}
}

void trains_start(void) {
	create(PRIORITY_TRAINSRV, trains_server);
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
	struct trains_request req = { .type = QUERY_ACTIVE };
	int resp_size = send(trains_tid(), &req, sizeof(req), trains_out, MAX_ACTIVE_TRAINS * sizeof(int));
	return resp_size / sizeof(int);
}
void trains_query_spatials(int train, struct train_state *state_out) {
	TSEND2(((struct trains_request) {
		.type = QUERY_SPATIALS,
		 .train_number = train,
	}), state_out);
}

int trains_query_error(int train) {
	int error;
	TSEND2(((struct trains_request) {
		.type = QUERY_ERROR,
		 .train_number = train,
	}), &error);
	return error;
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

// Callers outside trainsrv should not call this
// It just has trainsrv issue a reverse command immediately,
// and does not slow down the train.
// We have this done by the trainserver so that we can keep track
// of the fact that the train points in the other direction
void trains_reverse_unsafe(int train) {
	TSEND(((struct trains_request) {
		.type = REVERSE_UNSAFE,
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

struct switch_state trains_get_switches(void) {
	struct switch_state switches;
	TSEND2(((struct trains_request) {
		.type = SWITCH_GET,
	}), &switches);
	return switches;
}

void trains_set_stopping_distance(int train_id, int stopping_distance) {
	TSEND(((struct trains_request) {
		.type = SET_STOPPING_DISTANCE,
		 .train_number = train_id,
		  .stopping_distance = stopping_distance,
	}));
}

int trains_get_stopping_distance(int train_id) {
	int stopping_distance;
	TSEND2(((struct trains_request) {
		.type = GET_STOPPING_DISTANCE,
		 .train_number = train_id,
	}), &stopping_distance);
	return stopping_distance;
}

int trains_get_last_known_sensor(int train_id) {
	int sensor;
	TSEND2(((struct trains_request) {
		.type = GET_LAST_KNOWN_SENSOR,
		 .train_number = train_id,
	}), &sensor);
	return sensor;
}
