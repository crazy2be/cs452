#pragma once 
#include "request_type.h"
#include "switch_state.h"

#define CONDUCTOR_NAME_LEN 20
int conductor(int train_id);
void conductor_get_name(int train_id, char (*buf)[CONDUCTOR_NAME_LEN]);

struct conductor_req {
	enum request_type type;
	union {
		struct {
			const struct track_node *dest;
		} dest;
		struct {
			int sensor_num;
			int time;
		} sensor;
		struct {
			int switch_num;
			enum sw_direction dir;
			int expected_time;
		} switch_timeout;
		struct {
			int expected_time;
		} stop_timeout;
	} u;
};
