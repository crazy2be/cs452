#pragma once

#include "../trainsrv.h"
#include "trainsrv_internal.h"

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

	//  5. For debug purposes, we maintain info about the next sensor we expect to hit
	//     When we hit the next sensor, this allows us to know how far off our estimates were
	const struct track_node *next_sensor;
	int mm_to_next_sensor; // can be negative, if we've passed the sensor
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

	struct switch_state switches;
	struct sensor_state sens_prev;
	int sensors_are_known;
};

int train_velocity_from_state(struct internal_train_state *train_state);
int train_velocity(struct trainsrv_state *state, int train);
struct position get_estimated_train_position(struct trainsrv_state *state,
		struct internal_train_state *train_state);

struct internal_train_state* get_train_state(struct trainsrv_state *state, int train_id);

// returns 1 if we should actually set the speed
int update_train_speed(struct trainsrv_state *state, int train_id, int speed);
// returns current speed of train
int update_train_direction(struct trainsrv_state *state, int train_id);
void update_sensors(struct trainsrv_state *state, struct sensor_state sens);
void update_switch(struct trainsrv_state *state, int sw, enum sw_direction dir);

void trains_init(struct trainsrv_state *state);
