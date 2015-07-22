#pragma once

#include "estimate_position.h"

struct attribution {
	const struct internal_train_state* train;
	// these two are undefined if train is null
	int changed_switch;
	int distance_travelled;
	bool reversed;
};
// Returns the train id of the train though to have hit the sensor, or
// NULL if the sensor is thought to have misfired
//
// If there are multiple trains on the track in unknown positions, we
// aren't able to determine which train hit which sensor.
// This shouldn't even be a legal state for the train server to be in,
// so we ignore it here, and assert against it elsewhere.
struct attribution attribute_sensor_to_train(struct trainsrv_state *state, int sensor, int now);
