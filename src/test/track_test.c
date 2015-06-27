#include "../user/track.h"
#include <assert.h>

void track_tests(void) {
	init_tracka(track);

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
