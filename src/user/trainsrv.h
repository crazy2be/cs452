#pragma once

#include "switch_state.h"
#include "calibrate/track_node.h"
#include "sensorsrv.h"

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

void trains_query_spatials(int train, struct train_state *state_out);
int trains_query_arrival_time(int train, int distance);
void trains_send_sensors(struct sensor_state state);
void trains_set_speed(int train, int speed);
void trains_reverse(int train);
void trains_switch(int switch_numuber, enum sw_direction d);
int start_trains(void);
