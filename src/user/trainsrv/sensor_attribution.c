#include "sensor_attribution.h"
#include "sensor_history.h"
#include "speed_history.h"

#include "../sys.h"
#include "../track.h"
#include "../displaysrv.h"
#include "../trainsrv.h"
#include "../polymath.h"

// We are willing to assume that the sensors / turnouts fail at most once - where
// we missed a sensor hit in the past, or where a turnout was switched in the wrong direction.
// We don't consider cases with multiple failures.
const int errors_assumed_threshold = 2;


// this is the maximum number of branches we can hit when traversing from
// one sensor to another, while hitting at most one sensor on the way
#define MAX_BRANCHES_IN_SEARCH 5

#define MAX_REVERSED_POSITIONS (MAX_ACTIVE_TRAINS * MAX_BRANCHES_IN_SEARCH)

struct reversed_train_position {
	int train_id;
	struct position position;
	int errors_assumed;
	int failed_switch;
};

struct reversed_train_context {
	const struct track_node *node;
	int errors_assumed;
	int failed_switch;
	int distance_to_go;
};

static int emit_candidate_reverse_position(struct reversed_train_context *context, int train_id, int edge_index,
		struct reversed_train_position *positions, int *positions_index) {
	if (context->distance_to_go < context->node->edge[edge_index].dist) {
		// we've arrived at the destination
		DEBUG("Might have stopped at position %s(%d) + %d mm" EOL, context->node->name, edge_index, context->distance_to_go);
		positions[(*positions_index)++] = (struct reversed_train_position) {
			.train_id = train_id,
			 .position = (struct position) { &context->node->edge[edge_index], context->distance_to_go },
			  .errors_assumed = context->errors_assumed,
			   .failed_switch = context->failed_switch,
		};
		return 1;
	} else {
		context->distance_to_go -= context->node->edge[edge_index].dist;
		context->node = context->node->edge[edge_index].dest;
		return 0;
	}
}

static int build_reversed_positions_for_train(const struct trainsrv_state *state, const struct internal_train_state *train,
		const struct track_node *start, int start_time, int end_time, int distance_travelled, struct reversed_train_position *positions) {
	// We assume that the switch states don't change from start_time to end_time
	// TODO: actually assert this
	const struct switch_state switches = switch_historical_get(&state->switch_history, start_time);

	int position_index = 0;

	struct reversed_train_context stack[MAX_BRANCHES_IN_SEARCH];
	int stack_size = 1;

	stack[0].node = start;
	stack[0].errors_assumed = 0;
	stack[0].failed_switch = -1;
	stack[0].distance_to_go = distance_travelled;

	while (stack_size > 0) {
		struct reversed_train_context context = stack[--stack_size];
		ASSERT(context.distance_to_go >= 0);
		int edge_index = 0;

		DEBUG("While reversing, at node %s, %d mm left" EOL, context.node->name, context.distance_to_go);

		switch (context.node->type) {
			case NODE_EXIT:
				// drop this on the floor
				continue;
			case NODE_BRANCH: {
				edge_index = switch_get(&switches, context.node->num);
				if (context.errors_assumed == 0) {
					struct reversed_train_context c2 = {
						context.node,
						1,
						context.node->num,
						context.distance_to_go,
					};
					if (!emit_candidate_reverse_position(&c2, train->train_id, !edge_index, positions, &position_index)) {
						stack[stack_size++] = c2;
					}
				}
				break;
			}
			case NODE_SENSOR:
				if (context.node != start) {
					if (context.errors_assumed != 0) continue;
					context.errors_assumed++;
				}
				break;
			default:
				break;
		}
		ASSERT(context.errors_assumed < errors_assumed_threshold);
		if (!emit_candidate_reverse_position(&context, train->train_id, edge_index, positions, &position_index)) {
			stack[stack_size++] = context;
		}
	}
	return position_index;
}

struct distance_context {
	const struct track_node *node;
	int errors_assumed;
	int failed_switch;
	int distance_travelled;
};

static int build_reversed_positions(const struct trainsrv_state *state,
		struct reversed_train_position *positions) {
	int out_index = 0;
	for (int i = 0; i < state->num_active_trains; i++) {
		const struct internal_train_state *ts = &state->train_states[i];
		// TODO: we also need to check if we've fully stopped & do the inverse of this check for the normal flow
		// probably extract this to a helper function
		if (!ts->reversed || ts->sensor_history.len <= 0) continue;

		// figure out the possible locations we could have stopped at

		// first, figure how far we've travelled since the last sensor
		DEBUG("Precomputing reverse positions for train %d" EOL, ts->train_id);
		bool found;
		for (int j = 1; j <= ts->speed_history.len - 1; j++) {
			struct speed_historical_kvp stopping_point = speed_historical_get_kvp_by_index(&ts->speed_history, j);
			// first, look for the the time at which we set the speed to zero
			if (stopping_point.st == 0) {
				// TODO: some of this should be factored into estimate_position
				struct speed_historical_kvp prev_speed = speed_historical_get_kvp_by_index(&ts->speed_history, j + 1);
				int prev2_speed = 0;
				if (j + 2 < ts->speed_history.len) {
					prev2_speed = speed_historical_get_by_index(&ts->speed_history, j + 2);
				}

				int velocity_index = prev_speed.st*2 + (prev2_speed >= prev_speed.st) - 1;
				int velocity = ts->est_velocities[velocity_index];

				int stopping_distance = ts->est_stopping_distances[velocity_index];

				struct curve_scaling scale = scale_deceleration_curve(velocity * fixed_point_scale / 1000,
						stopping_distance * fixed_point_scale, deceleration_model_coefs,
						acceleration_model_arity, stopping_time_coef);

				struct sensor_historical_kvp last_sensor = sensor_historical_get_kvp_current(&ts->sensor_history);
				const struct track_node *last_sensor_node = track_node_from_sensor(last_sensor.st);

				long long integral_start;
				long long integral_end = stopping_time_coef;
				int distance_left;

				if (last_sensor.time > stopping_point.time) {
					distance_left = 0;
					integral_start = (last_sensor.time - stopping_point.time) * scale.x_scale;
				} else {
					distance_left = velocity * (stopping_point.time - last_sensor.time);
					integral_start = 0;
				}

				if (integral_start < integral_end) {
					distance_left += integrate_polynomial(integral_start, integral_end,
							deceleration_model_coefs, deceleration_model_arity) / fixed_point_scale;
				}

				int stopping_time = stopping_point.time + stopping_time_coef / scale.x_scale;

				DEBUG("Train %d went %d mm past node %s, integrating from %d to %d, started stopping at %d, finished stopping at %d, last sensor at %d" EOL,
						ts->train_id, distance_left, last_sensor_node->name,
						(int) integral_start, (int) integral_end,
						stopping_point.time, stopping_time, last_sensor.time);


				int positions_found = build_reversed_positions_for_train(state, ts, last_sensor_node,
						last_sensor.time, stopping_time, distance_left, positions + out_index);
				DEBUG("Got %d potential positions" EOL, positions_found);
				out_index += positions_found;
				ASSERT(out_index < MAX_REVERSED_POSITIONS);

				found = true;

				break;
			}
		}
		ASSERT(found);

		// now, figure out the places the train could have ended up by travelling that distance
		// from the last sensor hit
	}
	return out_index;
}

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

struct train_candidate {
	const struct internal_train_state *train;
	int errors_assumed;
	int distance;
	int switch_to_adjust;
	bool reversed;
};

// evaluate which train is more likely to be responsible for a given sensor hit
static bool guess_improved(const struct trainsrv_state *state, int now,
		const struct train_candidate *old, const struct train_candidate *new) {
	if (old->train == NULL) {
		// no previous guess
		return true;
	} else if (old->errors_assumed > new->errors_assumed) {
		// always prefer a guess which assumes fewer track malfunctions
		return true;
	}

	int old_eta = train_eta_from_state(state, old->train, old->distance);
	int new_eta = train_eta_from_state(state, new->train, new->distance);

	// if some of the etas are unknown, prefer the known ones
	if (new_eta < 0 || old_eta < 0) {
		return old_eta < 0;
	}

	// otherwise, base guess on which matches the eta better
	int old_eta_error = abs((now - sensor_historical_get_kvp_current(&old->train->sensor_history).time) - old_eta);
	int new_eta_error = abs((now - sensor_historical_get_kvp_current(&new->train->sensor_history).time) - new_eta);

	DEBUG("Compared candidates: incumbent %d has eta err = %d, candidate %d has eta err = %d" EOL,
			old->train->train_id, old_eta_error, new->train->train_id, new_eta_error);

	return new_eta_error < old_eta_error;
}


static bool check_candidate_position_validity(const struct trainsrv_state *state, const struct search_context *context,
		struct train_candidate *candidate, int train_start_time) {
	// check if the train going up this path would have required going the wrong way over a switch
	int errors_assumed = context->errors_assumed + candidate->errors_assumed;
	int switch_to_adjust = candidate->switch_to_adjust;
	for (int m = 0; errors_assumed < errors_assumed_threshold && m < context->merges_hit_count; m++) {
		const struct track_edge *expected_branch_edge = context->merges_hit[m].branch_edge;
		const struct track_node *branch = expected_branch_edge->src;
		ASSERT(branch->type == NODE_BRANCH);

		const int distance_from_train = candidate->distance - context->merges_hit[m].distance;
		// TODO: this doesn't work correctly while the train is stopping, or under
		// acceleration, because train_eta_from_state doesn't do that
		const int estimated_time = train_start_time + train_eta_from_state(state, candidate->train, distance_from_train);
		const struct switch_state switches = switch_historical_get(&state->switch_history, estimated_time);
		const int direction = switch_get(&switches, branch->num);

		if (&branch->edge[direction] != expected_branch_edge) {
			ASSERTF(&branch->edge[!direction] == expected_branch_edge, "Unknown edge: %s->%s, expected %s->%s or %s->%s",
					expected_branch_edge->src->name, expected_branch_edge->dest->name,
					branch->edge[direction].src->name, branch->edge[direction].dest->name,
					branch->edge[!direction].src->name, branch->edge[!direction].dest->name);
			DEBUG("Train would have needed to go the wrong way at %s, switch direction was %d" EOL, branch->name, direction);
			errors_assumed++;
			// we think a switch is currently in the wrong position - we should
			// log this info to correct our internal model later
			// don't bother if the switch has changed state since the time in question
			int last_modified = switch_historical_get_kvp_current(&state->switch_history).time;
			DEBUG("Train estimated_time was %d, last_modified %d" EOL, estimated_time, last_modified);
			if (last_modified <= estimated_time) {
				switch_to_adjust = branch->num;
			}
		}
	}

	candidate->switch_to_adjust = switch_to_adjust;
	candidate->errors_assumed = errors_assumed;
	DEBUG("Errors assumed = %d" EOL, errors_assumed);
	// ignore the guess if it would require assuming too many errors
	return errors_assumed < errors_assumed_threshold;
}

struct attribution attribute_sensor_to_known_train(const struct trainsrv_state *state,
		const struct track_node *sensor, int now) {

	DEBUG("STARTING RUN" EOL);

	/* (void) build_reversed_positions; */
	struct reversed_train_position reversed_position[MAX_REVERSED_POSITIONS];
	int reversed_position_count = build_reversed_positions(state, reversed_position);
	DEBUG("reverse_position_count = %d" EOL, reversed_position_count);

	// this is much bigger than it needs to be - we should only need enough entries
	// so that we can merge in every possible way without hitting a sensor.
	const int queue_size = MAX_BRANCHES_IN_SEARCH;

	// queue of edges that we need to proceed backwards from.
	// this allows us to check both directions when traversing a switch.
	struct search_context queue[queue_size];
	int queue_len = 1;

	queue[0] = (struct search_context){ &sensor->reverse->edge[DIR_STRAIGHT], 0, 0 };

	struct train_candidate candidate = { NULL, 0, 0, -1, false };

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
		for (int i = 0; i < reversed_position_count; i++) {
			const struct internal_train_state *train_state = state->state_for_train[reversed_position[i].train_id - 1];
			DEBUG("Checking for reversed train %d at sensor %s (it's at %s)" EOL, train_state->train_id, node->name, reversed_position[i].position.edge->src->name);
			// we check against the src of the edge, and don't reverse, since these positions are pointed
			// in the opposite direction that the direction of a train headed towards our sensor
			if (reversed_position[i].position.edge->src == context.edge->src) {
				struct train_candidate new_candidate = { train_state, reversed_position[i].errors_assumed,
					reversed_position[i].position.displacement + context.distance, reversed_position[i].failed_switch, true };
				int start_time = -1;
				DEBUG("Found reversed train %d at %s" EOL, train_state->train_id, context.edge->src->name);
				for (int j = 2; j <= train_state->speed_history.len - 1; j++) {
					// first, look for the the time at which we set the speed to zero
					if (speed_historical_get_by_index(&train_state->speed_history, j) == 0) {
						start_time = speed_historical_get_kvp_by_index(&train_state->speed_history, j - 1).time;
					}
				}
				ASSERT(start_time != -1);
				if (!check_candidate_position_validity(state, &context, &new_candidate, start_time)) continue;
				if (guess_improved(state, now, &candidate, &new_candidate)) {
					DEBUG("Choosing train as new candidate" EOL);
					candidate = new_candidate;
				}
			}
		}
		if (node->type == NODE_SENSOR) {
			for (int i = 0; i < state->num_active_trains; i++) {
				// note that this also excludes the unknown position train, since last_sensor_hit = -1
				const struct internal_train_state *train_state = &state->train_states[i];
				int last_sensor_hit = sensor_historical_get_current(&train_state->sensor_history);
				if (last_sensor_hit == node->reverse->num) {
					// there is a train here
					DEBUG("Found train %d at sensor %s" EOL, train_state->train_id, node->name);

					const int last_sensor_hit_time = sensor_historical_get_kvp_current(&train_state->sensor_history).time;
					struct train_candidate new_candidate = { train_state, 0, context.distance, -1, false };
					if (!check_candidate_position_validity(state, &context, &new_candidate, last_sensor_hit_time)) continue;
					DEBUG("Errors are within tolerance" EOL);

					// we want to figure out if this is a better guess for which train hit
					// the sensor than any guess we might have so far
					if (guess_improved(state, now, &candidate, &new_candidate)) {
						DEBUG("Choosing train as new candidate" EOL);
						candidate = new_candidate;
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

	return (struct attribution) {
		candidate.train,
		candidate.switch_to_adjust,
		candidate.distance,
		candidate.reversed,
	};
}

// TODO: this should probably also return the distance the train was thought to have travelled,
// so we can delurk next_sensor & next_sensor_mm & simplify the actual_speed calculation
struct attribution attribute_sensor_to_train(struct trainsrv_state *state, int sensor, int now) {
	const struct track_node *sensor_node = track_node_from_sensor(sensor);
	struct attribution attr = attribute_sensor_to_known_train(state, sensor_node, now);
	if (attr.train == NULL && state->unknown_train_id > 0) {
		attr.train = get_train_state(state, state->unknown_train_id);
		attr.changed_switch = -1;
		attr.distance_travelled = 0;
		attr.reversed = false;
	}
	return attr;
}
