#include "../user/track.h"
#include "../user/trainsrv/estimate_position.h"
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

void track_tests(void) {
	init_tracka(track);
	test_next_sensor();
	test_actual_velocity();
}
