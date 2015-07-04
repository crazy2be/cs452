#pragma once

#include "track_node.h"
#include <assert.h>

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
