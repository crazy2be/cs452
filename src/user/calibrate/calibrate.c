#include "calibrate.h"
#include "../signal.h"
#include "../nameserver.h"
#include "../switch_state.h"
#include "../sensorsrv.h"
#include "track_node.h"
#include "track_data_new.h"
#include <util.h>

struct calibrate_req {
	enum { UPDATE_SENSOR, UPDATE_SWITCH } type;
	union {
		struct sensor_state sensors;
		struct switch_state switches;
	} u;
};

struct bookkeeping {
	const struct track_node *last_node;
	int distance_to_next_sensor;
	int time_at_last_sensor;
};

static const struct track_node* node_from_sensor(int sensor, const struct track_node *track) {
	for (int i = 0; i < TRACK_MAX; i++) {
		if (track[i].type == NODE_SENSOR && track[i].num == sensor) {
			return &track[i];
		}
	}
	ASSERTF(0, "Couldn't find sensor %d on the track", sensor);
	return NULL;
}

// Walk along the graph presented by the track, and calculate the distance between two nodes.
// It's assumed that we had a sensor hit at the start and end nodes, and the switches were in
// the given configuration.
// Therefore, we can trace out the path that the train would have taken given the orientation
// of the switches.
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

static void calculate_distance_travelled(int sensor, int time, const struct switch_state *switches,
		struct bookkeeping *bk, const struct track_node *track) {
	char buf[4];
	sensor_repr(sensor, buf);

	/* printf("Sensor %s was hit" EOL, buf); */

	const struct track_node *current = node_from_sensor(sensor, track);

	// we don't print a data point if we don't have a last position
	if (bk->last_node != NULL) {
		const int distance = distance_between_nodes(bk->last_node, current, switches);
		const int delta_t = time - bk->time_at_last_sensor;
		printf("%s, %s, %d, %d" EOL, bk->last_node->name, current->name, delta_t, distance);
	}

	bk->last_node = current;
	bk->time_at_last_sensor = time;

	/* printf("Current tracked sensor is %s, next is %s" EOL, n->name, bk->next_expected_node->name); */
}

static void handle_sensor_update(struct sensor_state *sensors, struct sensor_state *old_sensors,
		const struct switch_state *switches, struct bookkeeping *bk, const struct track_node *track) {

	for (int i = 0; i <= SENSOR_COUNT; i++) {
		int s = sensor_get(sensors, i);
		if (s && s != sensor_get(old_sensors, i)) {
			calculate_distance_travelled(i, sensors->ticks, switches, bk, track);
			return;
		}
	}
}

void start_calibrate(void) {
	register_as(CALIBRATESRV_NAME);
	signal_recv();

	struct sensor_state old_sensors;
	struct switch_state switches;
	memset(&switches, 0, sizeof(switches)); // mostly to make the compiler happy

	int has_switch_data = 0;
	int has_sensor_data = 0;
	int tid;
	struct bookkeeping bk;
	// need to set bk.next_expected_node = NULL, but the rest is to make the compiler happy
	memset(&bk, 0, sizeof(bk));

	struct track_node track[TRACK_MAX];
	init_tracka(track);

	struct calibrate_req req;

	for (;;) {
		receive(&tid, &req, sizeof(req));
		reply(tid, NULL, 0);

		switch (req.type) {
		case UPDATE_SENSOR:
			if (!has_switch_data) {
				printf("Discarding sensor data since we don't know the switch positions yet" EOL);
			} else if (!has_sensor_data) {
				printf("Getting sensor baseline" EOL);
				old_sensors = req.u.sensors;
				has_sensor_data = 1;
			} else {
				handle_sensor_update(&req.u.sensors, &old_sensors, &switches, &bk, track);
				old_sensors = req.u.sensors;
			}
			break;
		case UPDATE_SWITCH:
			// TODO: we don't really handle switches being toggled while the trains are running,
			// since the next sensor the train hits might be changed
			switches = req.u.switches;
			has_switch_data = 1;
			break;
		default:
			ASSERTF(0, "Unknown request type %d", req.type);
		}
	}
}

void calibratesrv(void) {
	int tid = create(HIGHER(PRIORITY_MIN, 1), start_calibrate);
	signal_send(tid);
}

void calibrate_send_sensors(int calibratesrv, struct sensor_state *st) {
	struct calibrate_req req;
	req.type = UPDATE_SENSOR;
	req.u.sensors = *st;
	ASSERT(0 == send(calibratesrv, &req, sizeof(req), NULL, 0));
}

void calibrate_send_switches(int calibratesrv, struct switch_state *st) {
	struct calibrate_req req;
	req.type = UPDATE_SWITCH;
	req.u.switches = *st;
	ASSERT(0 == send(calibratesrv, &req, sizeof(req), NULL, 0));
}
