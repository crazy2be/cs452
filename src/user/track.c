#include "track.h"
#include <assert.h>

struct track_node track[TRACK_MAX];

const struct track_node *track_node_from_sensor(int sensor) {
	for (int i = 0; i < TRACK_MAX; i++) {
		if (track[i].type == NODE_SENSOR && track[i].num == sensor) {
			return &track[i];
		}
	}
	ASSERTF(0, "Couldn't find sensor %d on the track", sensor);
	return NULL;
}


const struct track_node *track_go_forwards_generic(const struct track_node *cur,
        const struct switch_state *sw, break_cond cb, void *ctx, bool allow_cycles) {
	int iterations = 0;
	for (;;) {
		// If we've done more iterations than there are nodes in the track,
		// we must have cycles in our path, which indicates an error condition.
		// Just return NULL.
		if (!allow_cycles && iterations++ > TRACK_MAX) return NULL;
		else if (cur->type == NODE_EXIT) break;

		// choose which node we pass down to
		int index = cur->type == NODE_BRANCH &&
		            switch_get(sw, cur->num) == CURVED;

		const struct track_edge *e = &cur->edge[index];
		if (cb(e, ctx)) break;
		cur = e->dest;
	}
	return cur;
}

const struct track_node *track_go_forwards_cycle(const struct track_node *cur,
        const struct switch_state *sw, break_cond cb, void *ctx) {
	return track_go_forwards_generic(cur, sw, cb, ctx, true);
}

const struct track_node *track_go_forwards(const struct track_node *cur,
        const struct switch_state *sw, break_cond cb, void *ctx) {
	return track_go_forwards_generic(cur, sw, cb, ctx, false);
}

static bool break_on_sensor(const struct track_edge *e, void *ctx) {
	// break if we are leaving a sensor, and it's not the node we started at
	int *distance = (int*) ctx;
	if (e->src->type == NODE_SENSOR && *distance != 0) {
		return 1;
	} else {
		*distance += e->dist;
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

const struct track_node *find_track_node(const char *name) {
	for (int i = 0; i < ARRAY_LENGTH(track); i++) {
		if (strcmp(name, track[i].name) == 0) {
			return &track[i];
		}
	}
	WTF("Could not find track node %s", name);
	return NULL;
}
