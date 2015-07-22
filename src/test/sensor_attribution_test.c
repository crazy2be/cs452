#include "sensor_attribution_test.h"
#include "../user/trainsrv/estimate_position.h"
#include "../user/trainsrv/sensor_attribution.h"

static struct internal_train_state* init_train(struct trainsrv_state *state, int train_id, int sensor, int time, int velocity) {
	struct internal_train_state *train = &state->train_states[state->num_active_trains++];
	train->train_id = train_id;
	state->state_for_train[train_id - 1] = train;

	sensor_historical_init(&train->sensor_history);
	sensor_historical_set(&train->sensor_history, sensor, time);
	train->reversed = 0;

	speed_historical_init(&train->speed_history);
	speed_historical_set(&train->speed_history, 8, 0);

	train->est_velocities[0] = 0;
	train->est_stopping_distances[0] = 0;
	for (int i = 1; i < sizeof(train->est_velocities) / sizeof(train->est_velocities[0]); i++) {
		train->est_velocities[i] = velocity;
		train->est_stopping_distances[i] = 700;
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

		// we expect train A to hit this (travelling 300mm in ~ 60 ticks, and actually takes 50)
		struct attribution attr = attribute_sensor_to_train(&state, 46, 250);
		ASSERTF(train_a == attr.train, "%x != %x", (unsigned) train_a, (unsigned) attr.train); // C15

		// shouldn't attribute a totally random value to a sensor
		ASSERT(NULL == attribute_sensor_to_train(&state, 10, 250).train); // A11

		// shouldn't attribute to a train that just hit that sensor
		ASSERT(NULL == attribute_sensor_to_train(&state, 36, 250).train); // C6

		// should attribute a train, while assuming we've missed a single sensor
		// (went from E7 to D9, skipping D7)
		ASSERT(train_b == attribute_sensor_to_train(&state, 56, 500).train); // D9

		// shouldn't attribute sensor too far ahead of a train (can't go from E7 to E12,
		// skipping both D7 and D9)
		ASSERT(NULL == attribute_sensor_to_train(&state, 75, 250).train); // E12

		// allow getting one turnout wrong
		ASSERT(train_a == attribute_sensor_to_train(&state, 34, 250).train); // C3

		// don't allow getting one turnout wrong (this particular case is also equivalent to getting 2 wrong)
		ASSERT(NULL == attribute_sensor_to_train(&state, 74, 250).train); // E11
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

		// 1 bad sensor
		ASSERT(train_a == attribute_sensor_to_train(&state, 18, 250).train); // B3
		// 1 bad sensor & 1 bad turnout
		ASSERT(NULL == attribute_sensor_to_train(&state, 16, 250).train); // B1

		// 0 bad turnouts
		struct attribution attr = attribute_sensor_to_train(&state, 1, 250);
		ASSERT(train_b == attr.train); // A2
		ASSERT_INTEQ(attr.changed_switch, -1);

		// 1 bad turnout
		attr = attribute_sensor_to_train(&state, 13, 250);
		ASSERT(train_b == attr.train); // A14
		ASSERT_INTEQ(attr.changed_switch, 12);

		// 2 bad turnouts
		attr = attribute_sensor_to_train(&state, 14, 250);
		ASSERT(NULL == attr.train); // A15
		ASSERT_INTEQ(attr.changed_switch, -1);
	}

	{
		struct trainsrv_state state;
		init_state(&state);

		// train b is behind train a
		struct internal_train_state *train_a = init_train(&state, 63, 3, 200, 5000); // A4
		struct internal_train_state *train_b = init_train(&state, 58, 3, 300, 5000); // A4
		(void) train_b;

		ASSERT(train_a == attribute_sensor_to_train(&state, 31, 250).train); // B16
	}

	{
		struct trainsrv_state state;
		init_state(&state);

		struct internal_train_state *train_a = init_train(&state, 63, 3, 100, 5000); // A4
		speed_historical_set(&train_a->speed_history, 0, 200);
		sensor_historical_set(&train_a->sensor_history, 31, 360); // b16
		sensor_historical_set(&train_a->sensor_history, 36, 510); // c5
		speed_historical_set(&train_a->speed_history, 8, 580);
		train_a->reversed = true;

		ASSERT(train_a == attribute_sensor_to_train(&state, 37, 700).train); // C6

		// one sensor error
		ASSERT(train_a == attribute_sensor_to_train(&state, 30, 700).train); // B15

		// two sensor errors
		ASSERT(NULL == attribute_sensor_to_train(&state, 2, 700).train); // A3
	}

	{
		struct trainsrv_state state;
		init_state(&state);

		struct internal_train_state *train_a = init_train(&state, 63, 31, 100, 5000); // B16
		speed_historical_set(&train_a->speed_history, 0, 350);
		sensor_historical_set(&train_a->sensor_history, 36, 360); // C5
		speed_historical_set(&train_a->speed_history, 8, 800);
		train_a->reversed = true;

		// one sensor failure
		ASSERT(train_a == attribute_sensor_to_train(&state, 47, 1000).train); // C16
		// one switch failure
		ASSERT(train_a == attribute_sensor_to_train(&state, 39, 1000).train); // C8
	}

	{
		struct trainsrv_state state;
		init_state(&state);

		struct internal_train_state *train_a = init_train(&state, 63, 31, 100, 5000); // B16
		speed_historical_set(&train_a->speed_history, 0, 300);
		speed_historical_set(&train_a->speed_history, 8, 800);
		train_a->reversed = true;

		// one switch failure & one sensor missed
		ASSERT(NULL == attribute_sensor_to_train(&state, 39, 1000).train); // C8
	}

	// test stopping the train after passing the last sensor
	{
		struct trainsrv_state state;
		init_state(&state);

		struct internal_train_state *train_a = init_train(&state, 63, 71, 100, 3000); // E8
		for (int i = 1; i < sizeof(train_a->est_velocities) / sizeof(train_a->est_velocities[0]); i++) {
			train_a->est_stopping_distances[i] = 300;
		}

		speed_historical_set(&train_a->speed_history, 0, 150);
		speed_historical_set(&train_a->speed_history, 8, 800);
		train_a->reversed = true;

		// one switch failure & one sensor missed
		ASSERT(train_a == attribute_sensor_to_train(&state, 70, 1000).train); // E7
	}
}
