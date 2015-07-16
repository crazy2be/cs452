#include "sensor_attribution_test.h"
#include "../user/trainsrv/estimate_position.h"
#include "../user/trainsrv/sensor_attribution.h"

static struct internal_train_state* init_train(struct trainsrv_state *state, int train_id, int sensor, int time, int velocity) {
	struct internal_train_state *train = &state->train_states[state->num_active_trains++];
	train->train_id = train_id;
	state->state_for_train[train_id - 1] = train;

	train->last_sensor_hit = sensor;
	train->last_sensor_hit_time = time;

	train->current_speed_setting = 8;
	train->previous_speed_setting = 0;
	for (int i = 0; i < sizeof(train->est_velocities) / sizeof(train->est_velocities[0]); i++) {
		train->est_velocities[i] = velocity;
	}

	// NOTE: we leave a bunch of state uninitialized, since it shouldn't be used for this
	// test.
	// It's debatable whether it's better to leave it undefined, or zero it out, or define values.
	// Arguably, the best way to do this is to mock out the behaviour of the functions that
	// interact with it, but that's too fancy for C.

	return train;
}

static void init_state(struct trainsrv_state *state) {
	state->num_active_trains = 0;
	state->unknown_train_id = 0;
	switch_historical_init(&state->switch_history);
	struct switch_state switches = {};
	switch_historical_set(&state->switch_history, switches, 50);
	// sens_prev & sensors_are_known should not be used
}

void sensor_attribution_tests() {
	{
		struct trainsrv_state state;
		init_state(&state);

		struct internal_train_state *train_a = init_train(&state, 63, 36, 200, 5000); // C5
		struct internal_train_state *train_b = init_train(&state, 58, 70, 200, 5000); // E7
		int bad_switch;

		// we expect train A to hit this (travelling 300mm in ~ 60 ticks, and actually takes 50)
		ASSERT(train_a == attribute_sensor_to_train(&state, 46, 250, &bad_switch)); // C15

		// shouldn't attribute a totally random value to a sensor
		ASSERT(NULL == attribute_sensor_to_train(&state, 10, 250, &bad_switch)); // A11

		// shouldn't attribute to a train that just hit that sensor
		ASSERT(NULL == attribute_sensor_to_train(&state, 36, 250, &bad_switch)); // C6

		// should attribute a train, while assuming we've missed a single sensor
		// (went from E7 to D9, skipping D7)
		ASSERT(train_b == attribute_sensor_to_train(&state, 56, 500, &bad_switch)); // D9

		// shouldn't attribute sensor too far ahead of a train (can't go from E7 to E12,
		// skipping both D7 and D9)
		ASSERT(NULL == attribute_sensor_to_train(&state, 75, 250, &bad_switch)); // E12

		// allow getting one turnout wrong
		ASSERT(train_a == attribute_sensor_to_train(&state, 34, 250, &bad_switch)); // C3

		// don't allow getting one turnout wrong (this particular case is also equivalent to getting 2 wrong)
		ASSERT(NULL == attribute_sensor_to_train(&state, 74, 250, &bad_switch)); // E11
	}

	{
		struct trainsrv_state state;
		init_state(&state);

		struct internal_train_state *train_a = init_train(&state, 63, 31, 200, 5000); // B16
		struct internal_train_state *train_b = init_train(&state, 58, 45, 200, 5000); // C14

		struct switch_state switches = {};
		switch_set(&switches, 4, CURVED);
		switch_set(&switches, 15, CURVED);
		switch_set(&switches, 16, CURVED);
		switch_historical_set(&state.switch_history, switches, 51);

		int bad_switch;

		// 1 bad sensor
		ASSERT(train_a == attribute_sensor_to_train(&state, 18, 250, &bad_switch)); // B3
		// 1 bad sensor & 1 bad turnout
		ASSERT(NULL ==  attribute_sensor_to_train(&state, 16, 250, &bad_switch)); // B1

		// 0 bad turnouts
		ASSERT(train_b == attribute_sensor_to_train(&state, 1, 250, &bad_switch)); // A2
		ASSERT_INTEQ(bad_switch, -1);

		// 1 bad turnout
		ASSERT(train_b == attribute_sensor_to_train(&state, 13, 250, &bad_switch)); // A14
		ASSERT_INTEQ(bad_switch, 12);

		// 2 bad turnouts
		ASSERT(NULL == attribute_sensor_to_train(&state, 14, 250, &bad_switch)); // A15
		ASSERT_INTEQ(bad_switch, -1);
	}

	{
		struct trainsrv_state state;
		init_state(&state);

		// train b is behind train a
		struct internal_train_state *train_a = init_train(&state, 63, 3, 200, 5000); // A4
		struct internal_train_state *train_b = init_train(&state, 58, 3, 300, 5000); // A4
		(void) train_b;

		int bad_switch;

		ASSERT(train_a == attribute_sensor_to_train(&state, 31, 250, &bad_switch)); // B16
	}
}
