#pragma once

#include "calibrate/track_node.h"
#include "switch_state.h"

enum track_id { TRACK_A, TRACK_B };

// Position encodes a direction, since the nodes/edges in the track graph
// have a direction encoded implicitly.
// This means that there are two representations for the same *position* -
// one travelling in each direction.
struct position {
	struct track_edge *edge;

	// distance from the edge->src
	// invariant: 0 <= displacement <= edge->dist
	int displacement;
};

struct train_state {
	struct position position;

	// micrometers per clock tick?
	int velocity;
};

// starts positionsrv
void positionsrv(enum track_id track_id);

// client methods
void positionsrv_query_train_state(int train_id, struct state *state_out);

// distance is in micrometers
// arrival time is in clock ticks (absolute value, not relative to now)
int positionsrv_query_train_arrival_time(int train_id, int distance);

void positionsrv_notify_train_speed(int train_id, int speed);
void positionsrv_notify_train_reverse(int train_id);
void positionsrv_notify_switches(struct switch_state *switches);
void positionsrv_notify_sensors(struct sensor_state *sensors);
