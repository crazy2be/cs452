#pragma once

#include "switch_state.h"
#include "calibrate/track_node.h"
#include "sensorsrv.h"
#include "trainsrv/position.h"

enum track_id { TRACK_A, TRACK_B };


struct train_state {
	struct position position;

	// micrometers per clock tick?
	int velocity;
};

#define MAX_ACTIVE_TRAINS 8 // way more than we'll be able to have on the track in practice
// returns number of active trains (bounded above by MAX_ACTIVE_TRAINS)
// writes an array of active train ids to trains_out
int trains_query_active(int *trains_out);
void trains_query_spatials(int train, struct train_state *state_out);
int trains_query_arrival_time(int train, int distance);
void trains_send_sensors(struct sensor_state state);
void trains_set_speed(int train, int speed);
void trains_reverse(int train);
void trains_switch(int switch_numuber, enum sw_direction d);

void trains_start(void);

