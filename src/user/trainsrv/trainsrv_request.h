#pragma once
#include "../switch_state.h"
#include "../sensorsrv.h"
#include "../request_type.h"

// only exposed like this for testing :(

struct trains_request {
	enum request_type type;

	int train_number;
	int speed;

	int switch_number;
	enum sw_direction direction; // TODO: union?

	int distance;
	struct sensor_state sensors;
};
