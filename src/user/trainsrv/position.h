#pragma once

// Position encodes a direction, since the nodes/edges in the track graph
// have a direction encoded implicitly.
// This means that there are two representations for the same *position* -
// one travelling in each direction.
struct position {
	struct track_edge *edge;

	// distance from the edge->src
	// invariant: 0 <= displacement <= edge->dist
	int displacement;
};

int position_is_uninitialized(struct position *p);
void position_reverse(struct position *p);
