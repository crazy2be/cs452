#include "track.h"
#include <assert.h>

const struct track_node *track_node_from_sensor(int sensor) {
	for (int i = 0; i < TRACK_MAX; i++) {
		if (track[i].type == NODE_SENSOR && track[i].num == sensor) {
			return &track[i];
		}
	}
	ASSERTF(0, "Couldn't find sensor %d on the track", sensor);
	return NULL;
}


const struct track_node *track_go_forwards(const struct track_node *cur,
		const struct switch_state *sw, break_cond cb, void *ctx) {
	for (;;) {
		if (cur->type == NODE_EXIT) break;

		// choose which node we pass down to
		int index = cur->type == NODE_BRANCH &&
			switch_get(sw, cur->num) == CURVED;

		const struct track_edge *e = &cur->edge[index];
		if (cb(e, ctx)) break;
		cur = e->dest;
	}
	return cur;
}

static bool break_on_sensor(const struct track_edge *e, void *ctx) {
	// break if we are leaving a sensor, and it's not the node we started at
	if (e->src->type == NODE_SENSOR) {
		int *distance = (int*) ctx;
		*distance += e->dist;
		return *distance != e->dist;
	} else {
		return 0;
	}
}

const struct track_node *track_next_sensor(const struct track_node *cur,
		const struct switch_state *sw, int *distance_out) {
	int distance = 0;
	const struct track_node *next = track_go_forwards(cur, sw, break_on_sensor, &distance);
	*distance_out = distance;
	return next;
}
