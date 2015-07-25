#pragma once
#include "../routesrv.h"

enum poi_type { NONE, SWITCH, STOPPING_POINT };

// A point of interest is a sensor number, and a displacement.
// We're assuming that the train is travelling only on the specified path.
// If the sensor is negative, this means that the point of interest is
// before the first sensor on our path, and should be triggered immediately.
// Clearly, this doesn't work for the stopping position (ie: short moves don't
// work)
struct point_of_interest {
	int sensor_num;
	int delay;

	const struct track_node *original;
	int path_index; // internal use only, for ease of comparing poi
	int displacement; // internal use only, this is cheaper to compute than the delay

	enum poi_type type;
	union {
		struct {
			int num;
			enum sw_direction dir;
		} switch_info;
	} u;
};

struct poi_context {
	bool stopped;
	int poi_index; // for switches
};

struct conductor_state {
	int train_id;

	struct astar_node path[ASTAR_MAX_PATH];
	int path_len;
	int path_index;

	struct point_of_interest poi;
	struct poi_context poi_context;
	int last_sensor_time;

	// this is a hack so we can find poi that happen after we start stopping the train
	// we assume that the train is still travelling at the old speed, which doesn't matter
	// too much, since it just means the switches will get flipped a bit early
	int last_velocity;

	// keep track of current velocity & prev velocity
	// in order to do acceleration curves
	/* int cur_velocity, prev_velocity; */
};
