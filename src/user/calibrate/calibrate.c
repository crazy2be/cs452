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
	const struct track_node *last_node, *next_expected_node;
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

static const struct track_node* next_expected_sensor(const struct track_node *n,
		const struct switch_state *switches, int *distance) {
	do {
		ASSERT(n->type != NODE_EXIT); // we don't want to hit a dead end

		int index = 0;
		if (n->type == NODE_BRANCH && switch_get(switches, n->num) == CURVED) {
			index = 1;
		}
		const struct track_edge *edge = &n->edge[index];
		*distance += edge->dist;
		n = edge->dest;
	} while (n->type != NODE_SENSOR);
	return n;
}

static void calculate_distance_travelled(int sensor, int time, const struct switch_state *switches,
		struct bookkeeping *bk, const struct track_node *track) {
	const struct track_node *n;
	if (bk->next_expected_node == NULL) {
		// first node we get, so we need to figure out where we are
		// we don't print a data point here
		n = node_from_sensor(sensor, track);
	} else {
		n = bk->next_expected_node;
		ASSERT(n == node_from_sensor(sensor, track));
		int delta_t = time - bk->time_at_last_sensor;
		printf("%s -> %s : %d , %d", bk->last_node->name, n->name, delta_t, bk->distance_to_next_sensor);
	}

	bk->last_node = n;
	bk->next_expected_node = next_expected_sensor(n, switches, &bk->distance_to_next_sensor);
	bk->time_at_last_sensor = time;
}

static void handle_sensor_update(struct sensor_state *sensors, struct sensor_state *old_sensors,
		const struct switch_state *switches, struct bookkeeping *bk, const struct track_node *track) {

	for (int i = 1; i <= 18; i++) {
		calculate_distance_travelled(i, sensors->ticks, switches, bk, track);
		return;
	}
	for (int i = 153; i <= 156; i++) {
		calculate_distance_travelled(i, sensors->ticks, switches, bk, track);
		return;
	}
}

void start_calibrate(void) {
	register_as(CALIBRATESRV_NAME);
	signal_recv();

	struct sensor_state old_sensors;
	struct switch_state switches;
	memset(&switches, 0, sizeof(switches)); // mostly to make the compiler happy

	int has_switch_data = 0;
	int tid;
	struct bookkeeping bk;
	// need to set bk.next_expected_node = NULL, but the rest is to make the compiler happy
	memset(&bk, 0, sizeof(bk));

	struct track_node track[TRACK_MAX];
	init_trackb(track);

	struct calibrate_req req;

	for (;;) {
		receive(&tid, &req, sizeof(req));
		reply(tid, NULL, 0);

		switch (req.type) {
		case UPDATE_SENSOR:
			if (!has_switch_data) {
				printf("Discarding sensor data since we don't know the switch positions yet" EOL);
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
