#pragma once

#include "estimate_position.h"

// Returns the train id of the train though to have hit the sensor, or
// -1 if the sensor is thought to have misfired
//
// If there are multiple trains on the track in unknown positions, we
// aren't able to determine which train hit which sensor.
// This shouldn't even be a legal state for the train server to be in,
// so we ignore it here, and assert against it elsewhere.
int attribute_sensor_to_train(struct trainsrv_state *state, int sensor);
