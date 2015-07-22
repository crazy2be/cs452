#pragma once

#include "track_node.h"
#include <assert.h>
#include "../switch_state.h"

// Position encodes a direction, since the nodes/edges in the track graph
// have a direction encoded implicitly.
// This means that there are two representations for the same *position* -
// one travelling in each direction.
struct position {
	const struct track_edge *edge;

	// distance from the edge->src
	// invariant: 0 <= displacement <= edge->dist
	int displacement;
};

int position_is_uninitialized(const struct position *p);
void position_reverse(struct position *p);
int position_is_wellformed(struct position *p);

void position_travel_forwards(struct position *position, int distance, const struct switch_state *switches);

// return -1 if there is no path from start -> end
int position_distance_apart(const struct position *start, const struct position *end,
		const struct switch_state *switches);

struct position position_calculate_stopping_position(const struct position *current,
		const struct position *target, int stopping_distance, const struct switch_state *switches);

void position_repr(const struct position p, char *buf);
