#include "calibrate.h"
#include "../signal.h"
#include "../nameserver.h"
#include "../switch_state.h"
#include "../sensorsrv.h"
#include "../track.h"
#include "track_node.h"
#include "track_data_new.h"
#include "../trainsrv.h"
#include "../track.h"
#include <util.h>

struct calibrate_req {
	enum { UPDATE_SENSOR, UPDATE_SWITCH, DELAY_PASSED } type;
	union {
		struct sensor_state sensors;
		struct switch_state switches;
	} u;
};

struct bookkeeping {
	const struct track_node *last_node;
	int distance_to_next_sensor;
	int time_at_last_sensor;
	int train_speed_idx;
	bool waiting_for_warmup;
};

#define CALIB_TRAIN_NUMBER 62
static int s_train_speeds[] = {0, 8, 9, 10, 11, 12, 13, 14, 13, 12, 11, 10, 9, 8};

// Walk along the graph presented by the track, and calculate the distance
// between two nodes. It's assumed that we had a sensor hit at the start and
// end nodes, and the switches were in
// the given configuration.
// Therefore, we can trace out the path that the train would have taken given
// the orientation of the switches.
static int distance_between_nodes(const struct track_node *src,
		const struct track_node *dst, const struct switch_state *switches) {
	// we expect to periodically skip some sensors because of misfiring sensors
	// we allow ourselves to skip at most 1 sensor in a row before throwing up our hands
	const int max_missed_sensors = 1;
	int missed_sensor_tolerance = max_missed_sensors;
	int distance = 0;

	const struct track_node *current = src;

	for (;;) {
		/* printf("Currently at node %s" EOL, n->name); */

		// we hit a sensor, which is past this point - this point can't possibly be a dead end
		// if this happens, we throw up our hands and fail
		if (current->type == NODE_EXIT) {
			printf("ERROR: map data indicated that the train went into a dead end, but we hit a sensor..." EOL);
			return -1;
		}

		// choose which node we pass down to
		int index = 0;
		if (current->type == NODE_BRANCH && switch_get(switches, current->num) == CURVED) {
			index = 1;
		}

		const struct track_edge *edge = &current->edge[index];
		distance += edge->dist;
		current = edge->dest;

		if (current == dst) {
			break;
		} else if (current->type == NODE_SENSOR && --missed_sensor_tolerance < 0) {
			printf("ERROR: sensor data indicates that we've missed more than %d sensors" EOL, max_missed_sensors);
			return -1;
		}
	}
	return distance;
}

static int handle_sensor_update(struct sensor_state *sensors, struct sensor_state *old_sensors) {
	int sensor_changed = -1;
	for (int i = 0; i <= SENSOR_COUNT; i++) {
		int s = sensor_get(sensors, i);
		if (s && s != sensor_get(old_sensors, i)) {
			ASSERTF(sensor_changed == -1, "%d %d", i, sensor_changed);
			sensor_changed = i;
		}
	}
	return sensor_changed;
}

#include "../clockserver.h"
static void delay_task(void) {
	int delay_amount = -1, tid = -1;
	receive(&tid, &delay_amount, sizeof(delay_amount));
	reply(tid, NULL, 0);
	//printf("Delaying for %d %d"EOL, delay_amount, tid);
	delay(delay_amount);
	//printf("Delayed for %d %d"EOL, delay_amount, tid);
	struct calibrate_req req = (struct calibrate_req) { .type = DELAY_PASSED };
	send(tid, &req, sizeof(req), NULL, 0);
}
static void enque_delay(int amount) {
	int tid = create(PRIORITY_MAX, delay_task); // TODO: What priority?
	//printf("Created delay task with tid %d"EOL, tid);
	printf("Delaying for %d"EOL, amount);
	send(tid, &amount, sizeof(amount), NULL, 0);
}
static bool next_speed(struct bookkeeping *bk) {
	bk->train_speed_idx++;
	if (bk->train_speed_idx >= ARRAY_LENGTH(s_train_speeds)) return true;
	printf("Changing from speed %d to speed %d..."EOL,
		   s_train_speeds[bk->train_speed_idx - 1], s_train_speeds[bk->train_speed_idx]);
	trains_set_speed(CALIB_TRAIN_NUMBER, s_train_speeds[bk->train_speed_idx]);
	enque_delay(10*100); // 10 seconds
	bk->waiting_for_warmup = true;
	return false;
}

// for each speed {
//   set_speed to speed
//   warm up for 10 seconds
//   run for 60 seconds {
//     when we hit a sensor {
//       print sensor_from, sensor_to, speed, time, distance
void start_calibrate(void) {
	register_as(CALIBRATESRV_NAME);
	signal_recv();

	struct sensor_state old_sensors = {};
	struct switch_state switches = {};

	bool has_switch_data = false;
	struct bookkeeping bk = {};

	printf("Starting up..."EOL);
	next_speed(&bk);
	for (;;) {
		struct calibrate_req req = {};
		int tid = -1;
		receive(&tid, &req, sizeof(req));
		reply(tid, NULL, 0);
		if (req.type == UPDATE_SWITCH) {
			ASSERT(!has_switch_data); // We can't switch while it's running!
			switches = req.u.switches;
			has_switch_data = true;
			printf("Got switch data..."EOL);
		} else if (bk.waiting_for_warmup) {
			if (req.type == DELAY_PASSED) {
				enque_delay(60*100); // 60 seconds
				bk.waiting_for_warmup = false;
				printf("Warmup done!"EOL);
			} // Drop others
		} else if (req.type == UPDATE_SENSOR) {
			ASSERT(has_switch_data);
			int changed = handle_sensor_update(&req.u.sensors, &old_sensors);
			if (changed == -1) continue;
			///char buf[4];
			//sensor_repr(changed, buf);
			//printf("Sensor %s was hit" EOL, buf);

			const struct track_node *current = track_node_from_sensor(changed);
			if (bk.last_node == NULL) {
				// we don't print a data point if we don't have a last position
				bk.last_node = current;
				bk.time_at_last_sensor = req.u.sensors.ticks;
				old_sensors = req.u.sensors;
				continue;
			}

			const int distance = distance_between_nodes(bk.last_node, current, &switches);
			const int delta_t = req.u.sensors.ticks - bk.time_at_last_sensor;
			printf("data: %d, %d, %s, %s, %d, %d"EOL,
				s_train_speeds[bk.train_speed_idx - 1],
				s_train_speeds[bk.train_speed_idx],
				bk.last_node->name, current->name, delta_t, distance);
		} else if (req.type == DELAY_PASSED) {
			if (!next_speed(&bk)) break;
		} else {
			WTF("Unknown request type %d", req.type);
		}
	}
	printf("Done calibration!");
	trains_set_speed(CALIB_TRAIN_NUMBER, 0);
}

void calibratesrv(void) {
	int tid = create(HIGHER(PRIORITY_MIN, 1), start_calibrate);
	signal_send(tid);
}
void calibrate_send_sensors(int calibratesrv, struct sensor_state *st) {
	struct calibrate_req req;
	req.type = UPDATE_SENSOR;
	req.u.sensors = *st;
	send(calibratesrv, &req, sizeof(req), NULL, 0);
}
void calibrate_send_switches(int calibratesrv, struct switch_state *st) {
	struct calibrate_req req;
	req.type = UPDATE_SWITCH;
	req.u.switches = *st;
	send(calibratesrv, &req, sizeof(req), NULL, 0);
}
