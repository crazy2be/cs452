#include "../user/track.h"
#include "../user/trainsrv/estimate_position.h"
#include "../user/trainsrv/train_alert_srv.h"
#include "../user/nameserver.h"
#include "../user/displaysrv.h"
#include "../user/signal.h"
#include "../user/servers.h"
#include <assert.h>

static void test_next_sensor(void) {
	const struct track_node* e1 = &track[64];

	struct switch_state switches;
	memset(&switches, 0, sizeof(switches));
	switch_set(&switches, 154, STRAIGHT);
	switch_set(&switches, 153, CURVED);

	int distance;
	const struct track_node* c1 = track_next_sensor(e1, &switches, &distance);

	ASSERT(c1 == &track[32]);
	const int expected_distance = 239 + 246;
	ASSERT(distance == expected_distance);
}

int calculate_actual_velocity(struct internal_train_state *train_state,
		const struct track_node *sensor_node, const struct switch_state *switches, int ticks);

static void test_actual_velocity(void) {
	const struct track_node *b8, *c15, *d12;
	b8 = &track[23];
	c15 = &track[46];
	d12 = &track[59];

	struct internal_train_state ts;
	memset(&ts, 0, sizeof(ts));
	struct switch_state switches;
	memset(&switches, 0, sizeof(switches));
	int actual_velocity;

	ASSERT(-1 == calculate_actual_velocity(&ts, d12, &switches, 67)); // test uninitialized case

	// normal case (start exactly on a sensor)
	ts.last_known_position.edge = &c15->edge[0];
	ts.last_known_position.displacement = 0;
	ts.last_known_time = 0;
	actual_velocity = calculate_actual_velocity(&ts, d12, &switches, 100);
	ASSERTF(4040 == actual_velocity, "actual_velocity = %d", actual_velocity);

	// start at a non-zero displacement
	ts.last_known_position.edge = &c15->edge[0];
	ts.last_known_position.displacement = 4;
	ts.last_known_time = 0;
	ASSERT(4000 == calculate_actual_velocity(&ts, d12, &switches, 100));

	ASSERT(-1 == calculate_actual_velocity(&ts, d12, &switches, 0)); // 0 delta_t case

	(void) b8;
	// trip a sensor at a position not reachable from the previous position (the teleport case)
	ts.last_known_position.edge = &b8->edge[0];
	ts.last_known_position.displacement = 0;
	ts.last_known_time = 0;
	ASSERT(-2 == calculate_actual_velocity(&ts, d12, &switches, 100));
}

void test_train_alert_client(void) {
	int ticks = train_alert_at(58, (struct position){ &track[95].edge[0], 50 });
	send(parent_tid(), &ticks, sizeof(ticks), NULL, 0);
}

// no-op displaysrv for testing
void stub_displaysrv(void) {
	register_as(DISPLAYSRV_NAME);
	signal_recv();
	for (;;) {
		int tid;
		receive(&tid, NULL, 0);
		reply(tid, NULL, 0);
	}
}

void test_train_alert_srv(void) {
	init_tracka(track);
	start_servers();

	int t = create(HIGHER(PRIORITY_MIN, 1), stub_displaysrv);
	signal_send(t);

	trains_start();

	create(PRIORITY_MAX, test_train_alert_client);

	// test case where switches are in the wrong orientation (and possibly change)

	struct sensor_state sensors;
	memset(&sensors, 0, sizeof(sensors));
	trains_send_sensors(sensors);
	trains_set_speed(58, 8);

	sensor_set(&sensors, 77, 1); // e14
	sensors.ticks = 50;
	trains_send_sensors(sensors);

	sensor_set(&sensors, 72, 1); // e9
	sensors.ticks = 100;
	trains_send_sensors(sensors);

	int ticks, tid;
	receive(&tid, &ticks, sizeof(ticks));
	reply(tid, NULL, 0);

	printf("Got an alert at time %d" EOL, ticks);

	stop_servers();
}

void track_tests(void) {
	init_tracka(track);
	test_next_sensor();
	test_actual_velocity();
}
