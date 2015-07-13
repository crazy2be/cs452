#include "sensor_attribution_test.h"
#include "../user/trainsrv/estimate_position.h"
#include "../user/trainsrv/sensor_attribution.h"

static struct internal_train_state* init_train(struct trainsrv_state *state, int train_id, int sensor, int time, int velocity) {
	struct internal_train_state *train = &state->train_states[state->num_active_trains++];
	train->train_id = train_id;
	state->state_for_train[train_id] = train;

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

void sensor_attribution_tests() {
	struct trainsrv_state state;
	state.num_active_trains = 0;

	struct internal_train_state *train_a = init_train(&state, 63, 36, 200, 5000); // C6
	struct internal_train_state *train_b = init_train(&state, 58, 70, 200, 5000); // E7

	(void) train_b;

	state.unknown_train_id = 0;
	switch_historical_init(&state.switch_history);
	struct switch_state switches = {};
	switch_historical_set(&state.switch_history, switches, 50);


	// sens_prev & sensors_are_known should not be used

	// we expect train A to hit this (travelling 300mm in ~ 60 ticks, and actually takes 50)
	ASSERT_INTEQ(train_a->train_id, attribute_sensor_to_train(&state, 46, 250)); // C15
}
