#include "sensor_attribution.h"

#include "../sys.h"
#include "../track.h"

// evaluate which train is more likely to be responsible for a given sensor hit
static bool guess_improved(struct trainsrv_state *state, int now,
		struct internal_train_state *old, int old_errors, int old_dist,
		struct internal_train_state *candidate, int candidate_errors, int candidate_dist) {
	if (old == NULL) {
		// no previous guess
		return true;
	} else if (old_errors > candidate_errors) {
		// always prefer a guess which assumes fewer track malfunctions
		return true;
	}

	// otherwise, base guess on which matches the eta better
	int old_eta_error = abs((now - old->last_sensor_hit_time) - train_eta_from_state(state, old, old_dist));
	int candidate_eta_error = abs((now - candidate->last_sensor_hit_time) - train_eta_from_state(state, candidate, candidate_dist));

	DEBUG("Compared candidates: incumbent %d has eta err = %d, candidate %d has eta err = %d" EOL,
			old->train_id, old_eta_error, candidate->train_id, candidate_eta_error);

	return candidate_eta_error < old_eta_error;
}

// this is the maximum number of branches we can hit when traversing from
// one sensor to another, while hitting at most one sensor on the way
#define MAX_BRANCHES_IN_SEARCH 5

struct search_context {
	const struct track_edge *edge;
	int distance;
	int errors_assumed;

	int merges_hit_count;
	struct {
		const struct track_edge *branch_edge;
		int distance;
	} merges_hit[MAX_BRANCHES_IN_SEARCH];
};

static int attribute_sensor_to_known_train(struct trainsrv_state *state, const struct track_node *sensor, int now) {
	// this is much bigger than it needs to be - we should only need enough entries
	// so that we can merge in every possible way without hitting a sensor.
	const int queue_size = MAX_BRANCHES_IN_SEARCH;

	// We are willing to assume that the sensors / turnouts fail at most once - where
	// we missed a sensor hit in the past, or where a turnout was switched in the wrong direction.
	// We don't consider cases with multiple failures.
	const int errors_assumed_threshold = 2;

	// queue of edges that we need to proceed backwards from.
	// this allows us to check both directions when traversing a switch.
	struct search_context queue[queue_size];
	int queue_len = 1;

	queue[0] = (struct search_context){ &sensor->reverse->edge[DIR_STRAIGHT], 0, 0 };

	struct internal_train_state *train_candidate = NULL;
	int train_candidate_errors_assumed = 0, train_candidate_distance = 0;
	DEBUG("STARTING RUN" EOL);

	// TODO: should write this to eliminate copying context back and forth repeatedly
	while (queue_len > 0) {
		struct search_context context = queue[--queue_len];

		context.distance += context.edge->dist;

		const struct track_node *node = context.edge->dest;

		// Check if a train is on this segment of the track.
		// When we look for trains, we look for the last sensor the train tripped - we don't care
		// about the currently estimated position.
		// This has two advantages:
		//  1) The estimation assumes that the turnouts are in the position that we set them in,
		//     but we want to be tolerant of the fact that this may not be the case.
		//  2) Reanchoring doesn't happen, so we don't need to worry about the case where the train
		//     is estimated to be *past* the sensor it just hit.
		DEBUG("Traversing through node %s" EOL, node->name);
		if (node->type == NODE_SENSOR) {
			for (int i = 0; i < state->num_active_trains; i++) {
				if (state->train_states[i].last_sensor_hit == node->reverse->num) {
					// there is a train here
					struct internal_train_state *train_state = &state->train_states[i];

					// check if the train going up this path would have required going the wrong way over a switch
					int errors_assumed = context.errors_assumed;
					DEBUG("Found train %d at sensor %s, assuming %d errors" EOL, train_state->train_id, node->name, errors_assumed);
					for (int m = 0; errors_assumed < errors_assumed_threshold && m < context.merges_hit_count; m++) {
						const struct track_edge *expected_branch_edge = context.merges_hit[m].branch_edge;
						const struct track_node *branch = expected_branch_edge->src;
						ASSERT(branch->type == NODE_BRANCH);

						const int distance_from_train = context.distance - context.merges_hit[m].distance;
						const int estimated_time = train_state->last_sensor_hit_time + train_eta_from_state(state, train_state, distance_from_train);
						const struct switch_state switches = switch_historical_get(&state->switch_history, estimated_time);
						const int direction = switch_get(&switches, branch->num);

						if (&branch->edge[direction] != expected_branch_edge) {
							ASSERTF(&branch->edge[!direction] == expected_branch_edge, "Unknown edge: %s->%s, expected %s->%s or %s->%s",
									expected_branch_edge->src->name, expected_branch_edge->dest->name,
									branch->edge[direction].src->name, branch->edge[direction].dest->name,
									branch->edge[!direction].src->name, branch->edge[!direction].dest->name);
							DEBUG("Train would have needed to go the wrong way at %s, switch direction was %d" EOL, branch->name, direction);
							errors_assumed++;
						}
					}

					DEBUG("Errors assumed = %d" EOL, errors_assumed);
					// ignore the guess if it would require assuming too many errors
					if (errors_assumed >= errors_assumed_threshold) continue;
					DEBUG("Errors within tolerance" EOL);

					// we want to figure out if this is a better guess for which train hit
					// the sensor than any guess we might have so far
					if (guess_improved(state, now,
							train_candidate, train_candidate_errors_assumed, train_candidate_distance,
							train_state, errors_assumed, context.distance)) {
						DEBUG("Choosing train as new candidate" EOL);
						train_candidate = train_state;
						train_candidate_errors_assumed = errors_assumed;
						train_candidate_distance = context.distance;
					}
				}
			}
			// Even if we find a train, we continue exploring this route until we pass the error tolerance.
			// I initially though that we shouldn't abandon the route after hitting a single train, since a train
			// behind this one would have had to go through the first train to hit the sensor.
			// This is actually not quite right.
			// There are some places on the track where a train can pass another if timed right,
			// where the track branches and joins with itself.
			// TODO: no, I was right - we should stop here if we saw a train
			// we'll catch the other case by traversing through the other path

			if (++context.errors_assumed >= errors_assumed_threshold) continue;
		} else if (node->type == NODE_MERGE) {
			// We're at a merge - this means the train would have gone through it the
			// other way, as a branch.
			// We want to know if to go towards the sensor, the train would have needed to go
			// over the switch the wrong way.
			// This requires knowing the position of the switch at a given time, which requires
			// knowing when we would expect the train to be there - this depends on the train.
			// We therefore need to do this check when we find the train, so we just acculumate
			// the necessary data when we pass a merge, and check it later.
			context.merges_hit[context.merges_hit_count].branch_edge = context.edge->reverse;
			context.merges_hit[context.merges_hit_count].distance = context.distance;
			context.merges_hit_count++;
		} else if (node->type == NODE_BRANCH) {
			// explore both branches
			queue[queue_len] = context;
			queue[queue_len].edge = &node->edge[DIR_CURVED];
			queue_len++;
		} else if (node->type == NODE_EXIT) {
			continue;
		}

		// traverse to the next branch
		queue[queue_len] = context;
		queue[queue_len].edge = &node->edge[DIR_STRAIGHT];
		queue_len++;

		ASSERT(0 <= queue_len && queue_len <= queue_size);
	}
	return (train_candidate != NULL) ? train_candidate->train_id : -1;
}

int attribute_sensor_to_train(struct trainsrv_state *state, int sensor, int now) {
	const struct track_node *sensor_node = track_node_from_sensor(sensor);
	int train_id = attribute_sensor_to_known_train(state, sensor_node, now);
	if (train_id == -1 && state->unknown_train_id > 0) {
		train_id = state->unknown_train_id;
		state->unknown_train_id = 0;
	}
	return train_id;
}
