#include "../user/track.h"
#include "../user/trainsrv/estimate_position.h"
#include "../user/trainsrv/train_alert_srv.h"
#include "../user/trainsrv/trainsrv_request.h"
#include "../user/sys.h"
#include "../user/displaysrv.h"
#include "../user/signal.h"
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

static struct position random_position(void) {
	for (;;) {
		const struct track_node *start = &track[rand() % TRACK_MAX];
		if (start->type == NODE_NONE || start->type == NODE_EXIT) continue;
		struct position position;
		position.edge = &start->edge[(start->type == NODE_BRANCH) ? rand() % 2 : 0];
		if (position.edge->dist == 0) continue; // these can't be represented as legal positions
		position.displacement = abs(rand()) % position.edge->dist;
		ASSERTF(position_is_wellformed(&position), "test position (%s, %d) is malformed", position.edge->src->name, position.displacement);
		return position;
	}
}

static void test_travel_forwards() {
	// test for specific cases
	{
		struct position position = { &track[61].edge[DIR_STRAIGHT], 211 };
		struct switch_state switches = { 0x1762a05c };
		position_travel_forwards(&position, 1779840282, &switches);
	}
	// do fuzz test
	for (int round = 0; round < 5; round++) {
		struct position position = random_position();

		int distance = abs(rand());
		ASSERT(distance >= 0);
		struct switch_state switches;
		switches.packed = rand();

		/* printf("Test case is position {node=%s, edge=%d, displacement=%d}, distance=%d, switches=%x" EOL, */
		/* 		start->name, edge_count, position.displacement, distance, switches.packed); */

		// make sure no assertions are tripped
		position_travel_forwards(&position, distance, &switches);
	}
}

static void test_calculate_stopping_position(void) {
	// do fuzz test
	for (int round = 0; round < 5; round++) {
		struct position start = random_position();
		struct position target = random_position();
		int stopping_distance = abs(rand()) % 2000;
		struct switch_state switches;
		switches.packed = rand();

		position_calculate_stopping_position(&start, &target, stopping_distance, &switches);
	}
}

struct train_alert_client_params {
	int train_id;
	struct position position;
};

void test_train_alert_client(void) {
	struct train_alert_client_params params;
	int tid;
	receive(&tid, &params, sizeof(params));
	reply(tid, NULL, 0);
	// wait until 50mm past MR8
	int ticks = train_alert_at(params.train_id, params.position);
	receive(&tid, NULL, 0);
	reply(tid, &ticks, sizeof(ticks));
}

int start_await_client(int train_id, struct position position) {
	struct train_alert_client_params params = { train_id, position };
	int tid = create(PRIORITY_MAX, test_train_alert_client);
	send(tid, &params, sizeof(params), NULL, 0);
	return tid;
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

void int_test_train_alert_srv(void) {
	// smoke test of trainsrv & train_alertsrv
	init_tracka(track);
	start_servers();

	int t = create(HIGHER(PRIORITY_MIN, 1), stub_displaysrv);
	signal_send(t);

	trains_start();

	int child = start_await_client(58, (struct position) {
		&track[95].edge[0], 50
	});

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

	int ticks;
	send(child, NULL, 0, &ticks, sizeof(ticks));

	printf("Finished train srv smoke test" EOL);

	stop_servers();
}

void stub_trainsrv_spatials(int train_id, struct train_state state) {
	struct trains_request req;
	int tid;
	receive(&tid, &req, sizeof(req));
	ASSERT_INTEQ(req.type, QUERY_SPATIALS);
	ASSERT_INTEQ(req.train_number, train_id);
	reply(tid, &state, sizeof(state));
}

void stub_trainsrv_arrival(int train_id, int distance, int ticks) {
	struct trains_request req;
	int tid;
	receive(&tid, &req, sizeof(req));
	ASSERT_INTEQ(req.type, QUERY_ARRIVAL);
	ASSERT_INTEQ(req.train_number, train_id);
	ASSERT_INTEQ(req.distance, distance);
	reply(tid, &ticks, sizeof(ticks));
}

void test_train_alert_srv(void) {
	init_tracka(track);
	start_servers();

	// we pretend to be the trainsrv to reply with fake positional data
	register_as("trains");

	struct switch_state switches;
	memzero(&switches);
	train_alert_start(switches, false);
	struct train_state state;
	int ticks, client, client2;

	// test alert request which will be started immediately
	client = start_await_client(58, (struct position) {
		&track[95].edge[0], 50
	});
	memzero(&state);
	stub_trainsrv_spatials(58, state);

	state.position = (struct position) {
		&track[57].edge[0], 0
	};
	train_alert_update_train(58, state.position);
	stub_trainsrv_arrival(58, track[57].edge[0].dist + 50, 100);

	send(client, NULL, 0, &ticks, sizeof(ticks));
	ASSERT_INTEQ(ticks, 100);

	// test alert request which will wait until the train moves into position
	// the train is currently at d10, we want to wait until it moves past d8 & e8
	// also test that multiple waiters on the same train works

	// currently at d10
	state.position = (struct position) {
		&track[57].edge[0], 0
	};

	client = start_await_client(58, (struct position) {
		&track[71].edge[0], 70
	}); // e8
	stub_trainsrv_spatials(58, state);

	client2 = start_await_client(58, (struct position) {
		&track[55].edge[0], 30
	}); // d8
	stub_trainsrv_spatials(58, state);

	// check that updating things does nothing if not on final approach
	train_alert_update_train_speed(58);
	switch_set(&switches, 9, 1);
	train_alert_update_switch(switches);


	// now at d8
	state.position = (struct position) {
		&track[55].edge[0], 0
	};
	train_alert_update_train(58, state.position);

	stub_trainsrv_arrival(58, 30, 60);

	// now at e8
	state.position = (struct position) {
		&track[71].edge[0], 0
	};
	train_alert_update_train(58, state.position);

	stub_trainsrv_arrival(58, 70, 150);

	send(client, NULL, 0, &ticks, sizeof(ticks));
	ASSERT_INTEQ(ticks, 150);

	send(client2, NULL, 0, &ticks, sizeof(ticks));
	ASSERT_INTEQ(ticks, 60);

	// test multiple waiters
	// TODO: implement and test switch flipping functionality

	printf("Finished train alert test" EOL);
	stop_servers();
}

void track_tests(void) {
	init_tracka(track);
	test_next_sensor();
	test_actual_velocity();
	test_travel_forwards();
	test_calculate_stopping_position();
}
