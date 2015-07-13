#include "sensor_attribution.h"

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

	return candidate_eta_error < old_eta_error;
}

struct search_context {
	const struct track_edge *edge;
	int distance;
	int errors_assumed;
};

static int find_train_behind_sensor(struct trainsrv_state *state, const struct track_node *sensor) {
	// this is much bigger than it needs to be - we should only need enough entries
	// so that we can merge in every possible way without hitting a sensor.
	const int queue_size = 10;

	// queue of edges that we need to proceed backwards from.
	// this allows us to check both directions when traversing a switch.
	struct search_context *queue[queue_size];
	int queue_len = 1;

	queue[0] = (struct search_context){ sensor->edge->reverse, 0, 0 };

	struct internal_train_state *train_candidate = NULL;
	int train_candidate_errors_assumed, train_candidate_distance;

	int now = time();

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
		if (node->type == NODE_SENSOR) {
			for (int i = 0; i < state->num_active_trains; i++) {
				if (state->train_states[i].last_sensor_hit == node->num) {
					// there is a train here - we want to figure out if this is a better guess for which train hit
					// the sensor than any guess we might have so far
					if (guess_improved(state, now,
							train_candidate, train_candidate_errors_assumed, train_candidate_distance,
							&state->train_states[i], context.errors_assumed, context.distance)) {
						train_candidate = state->train_states[i].train_id;
						train_candidate_errors_assumed = context.errors_assumed;
						train_candidate_distance = distance;
					}
				}
			}
			// Even if we find a train, we continue exploring this route until we pass the error tolerance.
			// I initially though that we shouldn't abandon the route after hitting a single train, since a train
			// behind this one would have had to go through the first train to hit the sensor.
			// This is actually not quite right.
			// There are some places on the track where a train can pass another if timed right,
			// where the track branches and joins with itself.

			candidate.errors_assumed++;
		} else if (node->type == NODE_MERGE) {
			// check if the train going up this path would have required going the
			// wrong way over a switch
			const struct track_node *rev = node->reverse;
			// TODO: this doesn't quite work - the estimated time is a function of the *train*, not the route
			// we need to do this check after we have the route calculated
			// this means that we'll need to accumulate a list of switches that we passed through.
			const int estimated_time;
			const struct switch_state switches = switch_historical_get(&state->switch_history, estimated_time);
			const int switch_state = switch_get(&switches, rev->num);
			if (&rev->edge[switch_state] != context.edge) {
				candidate.errors_assumed++;
			}
		} else if (node->type == NODE_BRANCH) {
			// explore both branches
			queue[queue_len++] = (struct search_context) { node->edge[DIR_CURVED], context.errors_assumed, context.distance };
		}

		// traverse to the next branch
		queue[queue_len++] = (struct search_context) { node->edge[DIR_STRAIGHT], context.errors_assumed, context.distance };

		ASSERT(0 <= queue_len && queue_len <= queue_size);
	}
}

// don't deal with the case where there is a train at an unknown position on the track
static int attribute_sensor_to_known_train(struct trainsrv_state *state, const struct track_node *sensor) {

	// travel backwards from the sensor
}

int attribute_sensor_to_train(struct trainsrv_state *state, int sensor) {
	const struct track_node *sensor_node = track_node_from_sensor(sensor);
	int train_id = attribute_sensor_to_known_train(state, sensor_node);
	if (train_id == -1 && state->unknown_train_id > 0) {
		train_id = state->unknown_train_id;
		state->unknown_train_id = 0;
	}
	return train_id;
}
