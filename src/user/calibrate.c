#include "calibrate.h"
#include "signal.h"
#include "sys.h"
#include "switch_state.h"
#include "sensorsrv.h"
#include "track.h"
#include "trainsrv/track_node.h"
#include "trainsrv/track_data_new.h"
#include "trainsrv/track_control.h"
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

#define CALIB_TRAIN_NUMBER 63
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
	for (int i = 0; i < SENSOR_COUNT; i++) {
		int s = sensor_get(sensors, i);
		if (!s || s == sensor_get(old_sensors, i)) continue;
		if (sensor_changed >= 0)
			printf("Warning: More than one sensor changed. Ignoring %d."EOL, sensor_changed);
		sensor_changed = i;
	}
	return sensor_changed;
}

static void delay_task(void) {
	int delay_amount = -1, tid = -1;
	receive(&tid, &delay_amount, sizeof(delay_amount));
	reply(tid, NULL, 0);
	delay(delay_amount);
	struct calibrate_req req = (struct calibrate_req) { .type = DELAY_PASSED };
	send(tid, &req, sizeof(req), NULL, 0);
}
static void enqueue_delay(int amount) {
	int tid = create(PRIORITY_MAX, delay_task); // TODO: What priority?
	printf("Delaying for %d"EOL, amount);
	send(tid, &amount, sizeof(amount), NULL, 0);
}
static bool next_speed(struct bookkeeping *bk) {
	bk->train_speed_idx++;
	if (bk->train_speed_idx >= ARRAY_LENGTH(s_train_speeds)) return false;
	printf("Changing from speed %d to speed %d..."EOL,
		   s_train_speeds[bk->train_speed_idx - 1], s_train_speeds[bk->train_speed_idx]);
	tc_set_speed(CALIB_TRAIN_NUMBER, s_train_speeds[bk->train_speed_idx]);
	enqueue_delay(10*100); // 10 seconds
	bk->waiting_for_warmup = true;
	return true;
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

	printf("Starting up..."EOL);
	struct bookkeeping bk = {};
	struct sensor_state old_sensors = {};
	struct switch_state switches;
	/* memset(&switches, 0xff, sizeof(switches)); */
	/* switch_set(&switches, 10, STRAIGHT); */
	/* switch_set(&switches, 13, STRAIGHT); */
	/* switch_set(&switches, 16, STRAIGHT); */
	/* switch_set(&switches, 17, STRAIGHT); */
	/* tc_switch_switches_bulk(switches); */
	// set up switches so that we make a loop around the outer track (for track A)
	memzero(&switches);
	switch_set(&switches, 11, CURVED);
	tc_switch_switches_bulk(switches);

	next_speed(&bk);
	for (;;) {
		struct calibrate_req req = {};
		int tid = -1;
		receive(&tid, &req, sizeof(req));
		reply(tid, NULL, 0);
		if (bk.waiting_for_warmup) {
			if (req.type == DELAY_PASSED) {
				enqueue_delay(60*100); // 60 seconds
				bk.waiting_for_warmup = false;
				printf("Warmup done!"EOL);
			} // Drop others
		} else if (req.type == UPDATE_SENSOR) {
			int changed = handle_sensor_update(&req.u.sensors, &old_sensors);
			if (changed == -1) continue;

			const struct track_node *current = track_node_from_sensor(changed);
			if (bk.last_node != NULL) {
				const int distance = distance_between_nodes(bk.last_node, current, &switches);
				const int delta_t = req.u.sensors.ticks - bk.time_at_last_sensor;
				printf("data: %d, %d, %s, %s, %d, %d"EOL,
				       s_train_speeds[bk.train_speed_idx - 1], s_train_speeds[bk.train_speed_idx],
				       bk.last_node->name, current->name, delta_t, distance);
			}

			// we don't print a data point if we don't have a last position
			bk.last_node = current;
			bk.time_at_last_sensor = req.u.sensors.ticks;
			old_sensors = req.u.sensors;
		} else if (req.type == DELAY_PASSED) {
			if (!next_speed(&bk)) break;
		} else {
			WTF("Unknown request type %d", req.type);
		}
	}
	printf("Done calibration!");
	tc_set_speed(CALIB_TRAIN_NUMBER, 0);
}

// *** Acceleration calibaration ***
// for acceleration calibration (on track B)
// runs from D8 -> C14, B16 -> E11
// for right now, lets just do D8 -> C14

struct acc_bookkeeping {
	int test_case_index;
	int trial_index;
	enum { WARMING_UP, ACCELERATING, STABLE_HI, DECELERATING, STABLE_LO, STOPPING, DONE } state;
	struct sensor_state old_sensors;
	int trial_start_time;
	int trial_start_sensor;
	int trial_end_sensor; // not used for stopping trials
};

struct speed_pair {
	int lo, hi;
};

#define MAX_TRIALS 15

static struct speed_pair acc_test_cases[] = { {0, 14} };


struct traversal_context {
	int dst;
	int dist;
};

bool traversal_cb(const struct track_edge *e, void *ctx_) {

	struct traversal_context *ctx = (struct traversal_context*) ctx_;
	if (e->src->num == ctx->dst) {
		return 1;
	} else {
		ctx->dist += e->dist;
		return 0;
	}
}

static int distance_between_sensors(int src, int dst, struct switch_state *switches) {
	struct traversal_context context = {dst, 0};
	const struct track_node *destn = track_go_forwards(track_node_from_sensor(src), switches, traversal_cb, &context);
	ASSERT(destn && destn->num == dst);
	return context.dist;
}

static int detect_run_start(int changed) {
	switch (changed) {
#if TRACKA
	/* case 51: // d4 -> b6 */
	/* 	return 22; */
	case 16: // b1 -> d14
		return 61;
#else
	case 31: // b16 -> d10
		return 57;
	case 52: // d5 -> a4
		return 3;
		break;
#endif
	default: return -1; // no run
	}
}

static void print_data_line(struct acc_bookkeeping *bk, int sensor, int now, struct switch_state *switches) {
	int distance = distance_between_sensors(bk->trial_start_sensor, sensor, switches);
	struct speed_pair test_case = acc_test_cases[bk->test_case_index];
	int v0, v1;
	if (bk->state == ACCELERATING) {
		v0 = test_case.lo;
		v1 = test_case.hi;
	} else {
		v0 = test_case.hi;
		v1 = test_case.lo;
	}
	printf("%d, %d, %d, %d" EOL, v0, v1, now - bk->trial_start_time, distance);
}

static void begin_next_trial(struct acc_bookkeeping *bk) {
	ASSERT(bk->state == ACCELERATING || bk->state == DECELERATING || bk->state == STOPPING);
	if (++bk->trial_index >= 2 * MAX_TRIALS) {
		bk->trial_index = 0;
		if (++bk->test_case_index >= sizeof(acc_test_cases) / sizeof(acc_test_cases[0])) {
			bk->state = DONE;
		} else {
			tc_set_speed(CALIB_TRAIN_NUMBER, acc_test_cases[bk->test_case_index].hi);
			bk->state = WARMING_UP;
			enqueue_delay(10 * 100); // 10 seconds
		}
	} else {
		if (bk->state == STOPPING) {
			tc_set_speed(CALIB_TRAIN_NUMBER, acc_test_cases[bk->test_case_index].hi);
		}
		// accelerating & stopping -> stable_hi, decelerating -> stable_lo
		bk->state = (bk->state != DECELERATING) ? STABLE_HI : STABLE_LO;
	}
}

void run_acc_calibration(void) {
	register_as(CALIBRATESRV_NAME);
	signal_recv();

	// we don't collect any velocity data here, just acceleration interval data
	// we can construct the whole picture by running both types of calibration
	// and combining the results during the data processing
	// set up switches so that we make a loop around the outer track (for track A)
	struct switch_state switches;
	memzero(&switches);
	switch_set(&switches, 15, CURVED);
	switch_set(&switches, 9, CURVED);
	tc_switch_switches_bulk(switches);

	struct acc_bookkeeping bk;
	memzero(&bk);

	tc_set_speed(CALIB_TRAIN_NUMBER, acc_test_cases[bk.test_case_index].hi);
	enqueue_delay(10*100); // 10 seconds
	printf("speed_0, speed_1, distance, time" EOL);

	while (bk.state != DONE) {
		int tid;
		struct calibrate_req req = {};
		receive(&tid, &req, sizeof(req));
		reply(tid, NULL, 0);
		if (bk.state == WARMING_UP) {
			if (req.type == DELAY_PASSED) {
				bk.state = STABLE_HI;
			}
			// drop others
		} else if (req.type == UPDATE_SENSOR) {
			int changed = handle_sensor_update(&req.u.sensors, &bk.old_sensors);
			if (changed == -1) continue;
			char buf[4];
			sensor_repr(changed, buf);
			printf("Passed sensor %s"EOL, buf);

			switch (bk.state) {
			case ACCELERATING:
			case DECELERATING:
				// end trial after specially chosen sensor
				if (changed != bk.trial_end_sensor) break;
			case STOPPING:
				// end trial after first sensor we hit while stopping
				print_data_line(&bk, changed, req.u.sensors.ticks, &switches);
				begin_next_trial(&bk);
				printf("Set state = %d" EOL, bk.state);
				break;
			case STABLE_LO:
			case STABLE_HI: {
				int next_run_end = detect_run_start(changed);
				if (next_run_end > 0) {
					bk.trial_start_sensor = changed;
					bk.trial_end_sensor = next_run_end;
					bk.trial_start_time = req.u.sensors.ticks;

					struct speed_pair test_case = acc_test_cases[bk.test_case_index];
					int speed = (bk.state == STABLE_LO) ? test_case.hi : test_case.lo;
					tc_set_speed(CALIB_TRAIN_NUMBER, speed);

					if (bk.state == STABLE_LO) {
						bk.state = ACCELERATING;
					} else if (speed == 0) {
						bk.state = STOPPING;
					} else {
						bk.state = DECELERATING;
					}
					printf("Set state = %d, speed = %d" EOL, bk.state, speed);
				}
				break;
			}
			default:
				break;
			}

			bk.old_sensors = req.u.sensors;
		}
	}
}


void calibratesrv(void) {
	/* int tid = create(HIGHER(PRIORITY_MIN, 1), start_calibrate); */
	int tid = create(HIGHER(PRIORITY_MIN, 1), run_acc_calibration);
	signal_send(tid);
}
void calibrate_send_sensors(int calibratesrv, struct sensor_state *st) {
	struct calibrate_req req;
	req.type = UPDATE_SENSOR;
	req.u.sensors = *st;
	send(calibratesrv, &req, sizeof(req), NULL, 0);
}
