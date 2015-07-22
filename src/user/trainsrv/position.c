#include "position.h"
#include <util.h>
#include "../track.h"

int position_is_uninitialized(const struct position *p) {
	return p->edge == NULL;
}

void position_reverse(struct position *p) {
	p->displacement = p->edge->dist - p->displacement;
	p->edge = p->edge->reverse;
}

int position_is_wellformed(struct position *p) {
	return p->displacement >= 0 && p->displacement < p->edge->dist;
}

// travel down the track for that many mm, and see which node we end at
static bool break_after_distance(const struct track_edge *e, void *ctx) {
	struct position *pos = (struct position*)ctx;

	pos->edge = e;

	// We move to the next edge so long as our displacement is greater than the
	// length of the edge we're currently on.
	if (pos->displacement >= e->dist) {
		pos->displacement -= e->dist;
		return 0;
	} else {
		return 1;
	}
}

// TODO: this should really pass the switches by value
void position_travel_forwards(struct position *position, int distance, const struct switch_state *switches) {
	ASSERT(distance >= 0);
	// Note that this violates the usual invariant of displacement < edge->dist
	// Here, displacement is really the total distance travelled by the train in
	// the time interval since we last knew it's position
	position->displacement += distance;

	// we special case the first edge like this since track_go_forwards takes a node, but we
	// start part of the way down an *edge*
	// If we didn't do this, if the starting node was a branch, we could be affected by the
	// direction of the switch even though we've passed the switch already
	if (position->displacement >= position->edge->dist) {
		position->displacement -= position->edge->dist;
		const struct track_node *node = track_go_forwards_cycle(position->edge->dest, switches, break_after_distance, position);
		ASSERT(node != NULL);

		// normalize positions that run off the end of the track
		if (position->displacement >= position->edge->dist) {
			ASSERT(node->type == NODE_EXIT);
			position->displacement = position->edge->dist - 1;
		}
	}

	ASSERTF(position_is_wellformed(position), "(%s, %d) is malformed", position->edge->src->name, position->displacement);
}

struct specific_node_context {
	const struct track_node *node;
	int distance;
};

static bool break_at_specific_node(const struct track_edge *e, void *context_) {
	struct specific_node_context *context = (struct specific_node_context*) context_;
	if (e->src == context->node) {
		return 1;
	} else {
		context->distance += e->dist;
		return 0;
	}
}

int position_distance_apart(const struct position *start, const struct position *end,
		const struct switch_state *switches) {

	struct specific_node_context context = { end->edge->src, end->displacement + start->edge->dist - start->displacement };

	const struct track_node *ending_node = track_go_forwards(start->edge->dest, switches, break_at_specific_node, &context);
	if (ending_node != end->edge->src) {
		// if there is no path from the last position to this node, either because
		// we entered a cycle, or hit an exit node
		ASSERT(!ending_node || ending_node->type == NODE_EXIT);
		return -1;
	}

	ASSERT(context.distance >= 0);

	return context.distance;
}

struct position position_calculate_stopping_position(const struct position *current,
		const struct position *target, int stopping_distance, const struct switch_state *switches) {

	// find distance from current train position
	const int current_distance = position_distance_apart(current, target, switches);
	if (current_distance < 0) {
		// no path to that position
		return (struct position){};
	}

	int distance = current_distance - stopping_distance;

	if (distance < 0) {
		// account for being too close to the stopping point, and needing to loop around to get
		// back to the same position / coast around to slow down
		const int loop_distance = position_distance_apart(target, target, switches);
		ASSERT(loop_distance > 0);
		do {
			distance += loop_distance;
		} while (distance < 0);
	}

	ASSERT(distance >= 0);

	struct position stopping_point = *current;
	position_travel_forwards(&stopping_point, distance, switches);
	return stopping_point;
}

void position_repr(const struct position p, char *buf) {
	int edge_index = &p.edge->src->edge[0] != p.edge;
	snprintf(buf, 20, "%s[%d] + %d", p.edge->src->name, edge_index, p.displacement);
}
