#pragma once

#include "../trainsrv.h"
#include "trainsrv_internal.h"
#include "speed_history.h"
#include "sensor_history.h"

extern const long long deceleration_model_coefs[6];
extern const unsigned deceleration_model_arity;
extern const long long stopping_time_coef;
extern const long long acceleration_model_coefs[6];
extern const unsigned acceleration_model_arity;

// state about what we know about this train
// (previous_speed_was_bigger, current)
// (true, 0), (false, 1), (true, 1), ..., (false, 13), (true, 13), (false, 14)
#define NUM_SPEED_SETTINGS 28
struct internal_train_state {
	//  1. Its current estimated position
	//     position_unitialized(&position) iff train_id == state->unknown_train_id
	//     "last known" is something of a misnomer - this changes when we reanchor the trains,
	//     so it is subject to estimation error if we reanchor.
	struct position last_known_position;
	int last_known_time;

	//  2. Our best guess for its velocity at each of the 28 speed settings
	//     (The speed setting behaves differently depending on if you accelerate or
	//     decelerate into that speed.
	//     You can only decelerate to speed 0, and accelerate to speed 14
	//     Therefore, 28 = 1 + 2 * 14 - 1.)
	int est_velocities[NUM_SPEED_SETTINGS];
	int est_stopping_distances[NUM_SPEED_SETTINGS];

	//  3. Its current and historical speed
	struct speed_historical_state speed_history;

	//  4. Later, we'll have information about if (and at what rate) it is currently
	//     accelerating

	//  5. For debug purposes, we maintain info about the next sensor we expect to hit
	//     When we hit the next sensor, this allows us to know how far off our estimates were
	const struct track_node *next_sensor;
	int mm_to_next_sensor; // can be negative, if we've passed the sensor

	// it's sometimes convenient to be able to go back from a state to a train_id
	int train_id;

	// mostly for reporting purposes
	// this, unlike last_known_position, is not affected by reanchoring
	struct sensor_historical_state sensor_history;
	// reversed is mostly a concern of the sensor attribution code
	// this is initially 0, and is logically inverted every time we reverse the train
	// when a sensor is attributed to this train, reversed is returned to 0
	int reversed;

	// amount estimate was off by the last time we hit a sensor
	int measurement_error;

	int conductor_tid;
};

// catch-all struct to avoid static memory allocation for a user space task
struct trainsrv_state {
	int displaysrv_tid;

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

	struct switch_historical_state switch_history;
	struct sensor_state sens_prev;
	int sensors_are_known;
};

int train_speed_index(const struct internal_train_state *train_state);
int train_velocity_from_state(const struct internal_train_state *train_state);
int train_velocity(struct trainsrv_state *state, int train);
struct position get_estimated_train_position(struct trainsrv_state *state,
        struct internal_train_state *train_state);
int train_eta_from_state(const struct trainsrv_state *state, const struct internal_train_state *train_state, int distance);
int train_eta(struct trainsrv_state *state, int train_id, int distance);

struct internal_train_state* get_train_state(struct trainsrv_state *state, int train_id);

// returns 1 if we should actually set the speed
int update_train_speed(struct trainsrv_state *state, int train_id, int speed);
// returns current speed of train
int update_train_direction(struct trainsrv_state *state, int train_id);
void update_sensors(struct trainsrv_state *state, struct sensor_state sens);
void update_switch(struct trainsrv_state *state, int sw, enum sw_direction dir);

void trainsrv_state_init(struct trainsrv_state *state);
