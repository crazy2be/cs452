#include "estimate_position.h"
#include "track_control.h"
#include "train_alert_srv.h"
#include "../track.h"
#include "../displaysrv.h"
#include "../sys.h"

int train_speed_index(struct internal_train_state *train_state) {
	int cur_speed = train_state->current_speed_setting;
	int i = cur_speed*2 + (train_state->previous_speed_setting >= cur_speed) - 1;
	ASSERT(i >= 0);
	ASSERT(i < sizeof(train_state->est_velocities) / sizeof(train_state->est_velocities[0]));
	return i;
}

int train_velocity_from_state(struct internal_train_state *train_state) {
	return train_state->est_velocities[train_speed_index(train_state)];
}

int train_eta(struct trainsrv_state *state, int train_id, int distance) {
	struct internal_train_state *train_state = get_train_state(state, train_id);
	ASSERT(train_state != NULL);
	int velocity = train_velocity_from_state(train_state);
	if (velocity > 0) {
		return distance / velocity;
	} else {
		return -1;
	}
}

int get_estimated_distance_travelled(struct internal_train_state *train_state, int now) {
	const int delta_t = now - train_state->last_known_time;
	const int velocity = train_velocity_from_state(train_state);

	const int distance = delta_t * velocity / 1000;

	const int overshoot_tolerance = 50;
	const int maximum_acceptable_distance = train_state->mm_to_next_sensor + overshoot_tolerance;

	return MIN(distance, maximum_acceptable_distance);
}

struct position get_estimated_train_position(struct trainsrv_state *state,
        struct internal_train_state *train_state) {

	struct position position = train_state->last_known_position;

	if (position_is_uninitialized(&position)) {
		// the position is unknown, so just return unknown
		return position;
	}

	// find how much time has passed since the recorded position on the internal
	// train state, and how far we expect to have moved since then
	position_travel_forwards(&position, get_estimated_distance_travelled(train_state, time()), &state->switches);

	return position;
}

struct internal_train_state* get_train_state(struct trainsrv_state *state, int train_id) {
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
		memset(train_state->est_velocities, 0, sizeof(train_state->est_velocities));
		return;
	}

	memset(train_state->est_stopping_distances, 0, sizeof(train_state->est_stopping_distances));

	// calculate wild ass guess of the train velocity table
	for (int i = 0; i < NUM_SPEED_SETTINGS; i++) {
		int speed = (i + 1) / 2;
		int is_accelerated = (i + 1) % 2;
		int velocity;
		if (speed < 8) {
			velocity = 0;
		} else {
			velocity = offset + speed * speed_coef + is_accelerated * is_accelerated_coef;
		}
		train_state->est_velocities[i] = velocity;
	}
}

static struct internal_train_state* allocate_train_state(struct trainsrv_state *state, int train_id) {
	ASSERT(get_train_state(state, train_id) == NULL);
	ASSERT(state->num_active_trains + 1 < ARRAY_LENGTH(state->train_states));
	struct internal_train_state *train_state = &state->train_states[state->num_active_trains++];
	memset(train_state, 0, sizeof(*train_state));
	state->state_for_train[train_id - 1] = train_state;
	train_state->train_id = train_id;

	// initialize estimated speeds based on train id
	//int train_scaling_factor = 0;
	initialize_train_velocity_table(train_state, train_id);

	return train_state;
}

// TODO: write handlers for functions trying to inspect state of train, find
// how long it will take to travel a particular distance, or get the set of
// currently active trains

static void reanchor(struct trainsrv_state *state,
                     struct internal_train_state *train_state) {
	int now = time();
	train_state->last_known_position = get_estimated_train_position(state, train_state);
	train_state->last_known_time = now;
	train_state->mm_to_next_sensor -= get_estimated_distance_travelled(train_state, now);
}

static void reanchor_all(struct trainsrv_state *state) {
	for (int i = 0; i < state->num_active_trains; i++) {
		reanchor(state, &state->train_states[i]);
	}
}

int update_train_speed(struct trainsrv_state *state, int train_id, int speed) {
	struct internal_train_state *train_state = get_train_state(state, train_id);
	if (train_state == NULL) {
		// we've never seen this train before
		ASSERT(state->unknown_train_id == 0);
		state->unknown_train_id = train_id;

		train_state = allocate_train_state(state, train_id);
	}
	// Just don't allow this so that we don't have to think about what the train
	// controller does in this case in terms of actual train speed.
	if (train_state->current_speed_setting == speed) return 0;
	reanchor(state, train_state); // TODO: Deacceration model.
	train_state->previous_speed_setting = train_state->current_speed_setting;
	train_state->current_speed_setting = speed;
	train_state->constant_speed_starts = time() + 400;
	return 1;
}

static bool position_unknown(struct trainsrv_state *state, int train_id) {
	ASSERT(train_id > 0);
	return state->unknown_train_id == train_id;
}

int update_train_direction(struct trainsrv_state *state, int train_id) {
	struct internal_train_state *train_state = get_train_state(state, train_id);
	if ((train_state != NULL) && (!position_unknown(state, train_id))) {
		position_reverse(&train_state->last_known_position);
	}
	// TODO: This is basically wrong.
	// We want to reanchor whenever we actually change speed, which happens
	// twice when we are reversing.
	reanchor(state, train_state);
	return train_state->current_speed_setting;
}

// called each time we hit a sensor
// prints out how far away we thought we were from the sensor at the time
// we hit it
static void log_position_estimation_error(const struct trainsrv_state *state,
        struct internal_train_state *train_state, const struct track_node *sensor_node, int ticks) {

	// if we had an estimated position for this train, ideally it should be estimated as
	// being very near to the tripped sensor when this happens
	// we will calculate how far away from the sensor we thought we were
	if (train_state->next_sensor != NULL) {
		char feedback[80];
		char sens_name[4];
		sensor_repr(sensor_node->num, sens_name);
		ASSERTF(train_state->next_sensor->type == NODE_SENSOR, "sensor node was %x from %x, track = %x", (unsigned) train_state->next_sensor, (unsigned) train_state, (unsigned) track);
		if (train_state->next_sensor == sensor_node) {
			const int delta_d = train_state->mm_to_next_sensor - get_estimated_distance_travelled(train_state, time());
			train_state->measurement_error = delta_d;
			/* snprintf(feedback, sizeof(feedback), "Train was estimated at %d mm away from sensor %s when tripped (%d total)", */
			/*          delta_d, sens_name, train_state->mm_to_next_sensor); */
			/* displaysrv_console_feedback(state->displaysrv_tid, feedback); */
		} else {
			char exp_sens_name[4];
			sensor_repr(train_state->next_sensor->num, exp_sens_name);
			snprintf(feedback, sizeof(feedback), "Train was expected to hit sensor %s, actually hit %s",
			         exp_sens_name, sens_name);
			displaysrv_console_feedback(state->displaysrv_tid, feedback);
		}
	}
	train_state->next_sensor = track_next_sensor(sensor_node, &state->switches,
	                           &train_state->mm_to_next_sensor);

	if (train_state->next_sensor->type != NODE_SENSOR) {
		ASSERT(train_state->next_sensor->type == NODE_EXIT);
		train_state->next_sensor = NULL;
	}
}

#define SILENT_ERROR -1
#define TELEPORT_ERROR -2
int calculate_actual_velocity(struct internal_train_state *train_state,
                              const struct track_node *sensor_node, const struct switch_state *switches, int ticks) {
	if (ticks < train_state->constant_speed_starts) return SILENT_ERROR;
	ASSERT(sensor_node != NULL);
	if (position_is_uninitialized(&train_state->last_known_position)) return SILENT_ERROR;

	struct position sensor_position = { &sensor_node->edge[0], 0 };
	const int actual_distance = position_distance_apart(&train_state->last_known_position, &sensor_position, switches);
	if (actual_distance < 0) return TELEPORT_ERROR;

	const int delta_t = ticks - train_state->last_known_time;
	if (delta_t == 0) return SILENT_ERROR;
	return (actual_distance * 1000) / delta_t;
}

static void update_train_velocity_estimate(const struct trainsrv_state *state, struct internal_train_state *train_state,
        const struct track_node *sensor_node, int ticks) {

	const int actual_velocity = calculate_actual_velocity(train_state, sensor_node, &state->switches, ticks);
	if (actual_velocity == TELEPORT_ERROR) {
		displaysrv_console_feedback(state->displaysrv_tid, "The train supposedly teleported");
		return;
	} else if (actual_velocity == SILENT_ERROR) {
		return;
	}

	// v' = (1 - alpha) * v + alpha * v_actual
	// empirically chosen to give a good update
	const int alpha = 100;
	const int divisor = 1000;

	/* char buf[80]; */
	/* snprintf(buf, sizeof(buf), "Updated velocity estimate is %d", actual_velocity); */
	/* displaysrv_console_feedback(state->displaysrv_tid, buf); */

	int *velocity_entry = &train_state->est_velocities[train_speed_index(train_state)];
	*velocity_entry = ((divisor - alpha) * *velocity_entry + alpha * actual_velocity) / divisor;
}

static void update_train_position_from_sensor(const struct trainsrv_state *state,
        struct internal_train_state *train_state,
        int sensor, int ticks) {

	const struct track_node *sensor_node = track_node_from_sensor(sensor);
	ASSERT(sensor_node != NULL && sensor_node->type == NODE_SENSOR && sensor_node->num == sensor);

	log_position_estimation_error(state, train_state, sensor_node, ticks);

	// adjust estimate for train speed
	update_train_velocity_estimate(state, train_state, sensor_node, ticks);

	// update internal train state
	train_state->last_known_position.edge = &sensor_node->edge[0];
	train_state->last_known_position.displacement = 0;
	train_state->last_known_time = ticks;
	train_state->last_sensor_hit = sensor;

	ASSERTF(position_is_wellformed(&train_state->last_known_position), "(%s, %d) is malformed",
			train_state->last_known_position.edge->src->name, train_state->last_known_position.displacement);

	// tell the train_alert server about this
	train_alert_update_train(train_state->train_id, train_state->last_known_position);
}

static void sensor_cb(int sensor, void *ctx) {
	int *sensor_dest= (int*) ctx;
	*sensor_dest = sensor;
}

void update_sensors(struct trainsrv_state *state, struct sensor_state sens) {
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

void update_switch(struct trainsrv_state *state, int sw, enum sw_direction dir) {
	reanchor_all(state);
	switch_set(&state->switches, sw, dir);
}

void trainsrv_state_init(struct trainsrv_state *state) {
	memset(state, 0, sizeof(*state));
	state->switches = tc_init_switches();
	state->displaysrv_tid = whois(DISPLAYSRV_NAME);
	displaysrv_update_switch(state->displaysrv_tid, &state->switches);
}
