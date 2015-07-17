#include "sensor_attribution.h"
#include "sensor_history.h"
#include "speed_history.h"

#include "../sys.h"
#include "../track.h"
#include "../displaysrv.h"
#include "../trainsrv.h"

// evaluate which train is more likely to be responsible for a given sensor hit
static bool guess_improved(const struct trainsrv_state *state, int now,
		const struct internal_train_state *old, int old_errors, int old_dist,
		const struct internal_train_state *candidate, int candidate_errors, int candidate_dist) {
	if (old == NULL) {
		// no previous guess
		return true;
	} else if (old_errors > candidate_errors) {
		// always prefer a guess which assumes fewer track malfunctions
		return true;
	}

	int old_eta = train_eta_from_state(state, old, old_dist);
	int candidate_eta = train_eta_from_state(state, candidate, candidate_dist);

	// if some of the etas are unknown, prefer the known ones
	if (candidate_eta < 0 || old_eta < 0) {
		return old_eta < 0;
	}

	// otherwise, base guess on which matches the eta better
	int old_eta_error = abs((now - sensor_historical_get_last_mod_time(&old->sensor_history)) - old_eta);
	int candidate_eta_error = abs((now - sensor_historical_get_last_mod_time(&candidate->sensor_history)) - candidate_eta);

	DEBUG("Compared candidates: incumbent %d has eta err = %d, candidate %d has eta err = %d" EOL,
			old->train_id, old_eta_error, candidate->train_id, candidate_eta_error);

	return candidate_eta_error < old_eta_error;
}

// this is the maximum number of branches we can hit when traversing from
// one sensor to another, while hitting at most one sensor on the way
#define MAX_BRANCHES_IN_SEARCH 5

#define MAX_REVERSED_POSITIONS (MAX_ACTIVE_TRAINS * MAX_BRANCHES_IN_SEARCH)

/* struct reversed_train_position { */
/* 	int train_id; */
/* 	struct position position; */
/* 	int errors_assumed; */
/* 	int failed_switch; */
/* }; */

/* struct reversed_train_context { */
/* 	const struct track_node *node; */
/* 	int errors_assumed; */
/* 	int failed_switch; */
/* 	int distance_to_go; */
/* } */

/* static int emit_candidate_reverse_position(struct reversed_train_context *context, int train_id, */
/* 		struct reversed_train_position *positions, int *positions_index) { */
/* 	if (context->distance_to_go < context->node->edge[DIR_STRAIGHT].dist) { */
/* 		// we've arrived at the destination */
/* 		positions[(*positions_index)++] = (struct reversed_train_position) { */
/* 			.train_id = train_id, */
/* 			 .position = (struct position) { &context->node->edge[DIR_STRAIGHT], context->distance_to_go }, */
/* 			  .errors_assumed = context->errors_assumed, */
/* 			   .failed_switch = context->failed_switch, */
/* 		}; */
/* 		return 1; */
/* 	} else { */
/* 		context->distance_to_go -= context->node->edge[DIR_STRAIGHT].dist; */
/* 		return 0; */
/* 	} */
/* } */

/* static int build_reversed_positions_for_train(const struct trainsrv_state *state, const struct internal_train_state *train, */
/* 		const struct track_node *start, int distance_travelled, struct reversed_train_position *positions) { */
/* 	int positions_index = 0; */

/* 	struct reversed_train_context stack[MAX_BRANCHES_IN_SEARCH]; */
/* 	int stack_size = 1; */

/* 	stack[0].node = starting_sensor_node; */
/* 	stack[0].errors_assumed = 0; */
/* 	stack[0].failed_switch = -1; */
/* 	stack[0].distance_to_go = distance_travelled; */

/* 	while (stack_size > 0) { */
/* 		struct reversed_train_context context = stack[--stack_size]; */

/* 		switch (context.node->type) { */
/* 			case NODE_EXIT: */
/* 				// drop this on the floor */
/* 				continue; */
/* 			case NODE_BRANCH: { */
/* 				// this is fucked */
/* 				if (context.errors_assumed == 0) { */
/* 					struct reversed_train_context c2 = { */
/* 						context.node->edges[DIR_CURVED].dest, */
/* 						1, */
/* 						context.node->num, */
/* 						context.distance_to_go, */
/* 					}; */
/* 					if (!emit_candidate_reverse_position(&c2, train->train_id, positions, &position_index)) { */
/* 						stack[stack_size++] = c2; */
/* 					} */
/* 				} */
/* 				break; */
/* 			} */
/* 			case NODE_SENSOR: */
/* 				if (context.errors_assumed != 0) continue; */
/* 				context.errors_assumed++; */
/* 				break; */
/* 		} */
/* 		context.node = context.node->edges[DIR_STRAIGHT].dest; */
/* 		if (!emit_candidate_reverse_position(&context, train->train_id, positions, &position_index)) { */
/* 			stack[stack_size++] = context; */
/* 		} */
/* 	} */
/* 	return position_index; */
/* } */

struct distance_context {
	const struct track_node *node;
	int errors_assumed;
	int failed_switch;
	int distance_travelled;
};

// finds the distance between two points on the track
// we're willing to go over one turnout the wrong way, or hit a single sensor
// even with these constraints, for our track, there should only be one such
// path between two sensors.
static int distance_between_points_error_tolerant(const struct trainsrv_state *state,
		const struct track_node *start, const struct track_node *end, int start_time, int end_time) {
	// we assume that the relevant switches do not change position betwen start_time & end_time,
	// since we have no way of knowing where the train is in the middle.
	// that would require having a model for the velocity as the train stops, which we
	// don't have right now
	// IF THIS ASSUMPTION IS VIOLATED, THIS WILL FAIL SILENTLY
	// We should assert that the assumption holds
	const struct switch_state switches = switch_historical_get(&state->switch_history, start_time);

	struct distance_context stack[MAX_BRANCHES_IN_SEARCH];
	int stack_size = 1;

	stack[0].node = starting_sensor_node;
	stack[0].errors_assumed = 0;
	stack[0].failed_switch = -1;

	int distance_candidate = -1;

	while (stack_size > 0) {
		struct reversed_train_context context = stack[--stack_size];

		if (context.node == end) {
			ASSERT(distance_candidate < 0);
			distance_candidate = context.distance_travelled;
			continue;
		}

		int edge_index = 0;

		const struct track_edge *e;
		switch (context.node->type) {
			case NODE_EXIT:
				// drop this on the floor
				continue;
			case NODE_BRANCH: {
				edge_index = switch_get(&switches, context.node->num);
				if (context.errors_assumed == 0) {
					e = &context.node->edges[!edge_index];
					stack[stack_size++] = (struct distance_context) {
						e->dest,
						1,
						context.node->num,
						context.distance_travelled + e->dist,
					};
				}
				break;
			}
			case NODE_SENSOR:
				if (context.errors_assumed != 0) continue;
				context.errors_assumed++;
				break;
		}

		e = &context.node->edges[edge_index];
		context.node = e->dest;
		context.distance_travelled += e->dist;
		stack[stack_size++] = context;
	}
	ASSERT(distance_candidate >= 0);
	return distance_candidate;
}

/* static int build_reversed_positions(const struct trainsrv_state *state, int now, */
/* 		struct reversed_train_position *positions) { */
/* 	int out_index = 0; */
/* 	for (int i = 0; i < state->num_active_trains; i++) { */
/* 		const struct internal_train_state *ts = &state->train_states[i]; */
/* 		if (ts->sensor_history.len <= 0 || ts->reversed == 0) continue; */

/* 		// figure out the possible locations we could have stopped at */

/* 		// first, figure how far we've travelled since the last sensor */

/* 		// TODO: this is an unapologetic hackjob */
/* 		// At the time of writing, we don't have an acceleration model, so we don't support anything */
/* 		// that would require having one. */
/* 		// We assume that the train was going at some constant velocity, and then came to a full stop */
/* 		bool found; */
/* 		for (int j = 1; j <= state->speed_history.len - 1; j++) { */
/* 			// first, look for the the time at which we set the speed to zero */
/* 			if (speed_historical_get_by_index(&state->speed_history, j) == 0) { */
/* 				// TODO: some of this should be factored into estimate_position */
/* 				int speed = speed_historical_get_by_index(&state->speed_history, j + 1); */
/* 				int prev_speed = 0; */
/* 				if (j + 2 < speed->speed_history.len) { */
/* 					prev_speed = speed_historical_get_by_index(&state->speed_history, j + 2); */
/* 				} */
/* 				int velocity_index = speed*2 + (prev_speed >= speed) - 1; */
/* 				int velocity = train_state->est_velocities[velocity_index]; */

/* 				int speed_time = speed_historical_get_mod_by_index(&state->speed_history, j + 1); */
/* 				int stop_time = speed_historical_get_mod_by_index(&state->speed_history, j); */

/* 				int stopping_distance = train_state->est_stopping_distances[velocity_index]; */
/* 				int distance_travelled = velocity * (speed_time - stop_time) + stopping_distance; */

/* 				out_index += build_reversed_positions_for_train(state, ts, stop_time, distance_travelled, positions + out_index); */
/* 				ASSERT(out_index < MAX_REVERSED_POSITIONS); */

/* 				found = true; */

/* 				break; */
/* 			} */
/* 		} */
/* 		ASSERT(found); */

/* 		// now, figure out the places the train could have ended up by travelling that distance */
/* 		// from the last sensor hit */
/* 	} */
/* } */

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

struct attribution attribute_sensor_to_known_train(const struct trainsrv_state *state,
		const struct track_node *sensor, int now) {

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

	const struct internal_train_state *train_candidate = NULL;
	int train_candidate_errors_assumed = 0, train_candidate_distance = 0;
	int train_candidate_switch_to_adjust = -1;
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
				// note that this also excludes the unknown position train, since last_sensor_hit = -1 
				const struct internal_train_state *train_state = &state->train_states[i];
				int last_sensor_hit = sensor_historical_get_current(&train_state->sensor_history);
				if (last_sensor_hit == node->reverse->num) {
					// there is a train here

					// check if the train going up this path would have required going the wrong way over a switch
					int errors_assumed = context.errors_assumed;
					int switch_to_adjust = -1;
					const int last_sensor_hit_time = sensor_historical_get_last_mod_time(&train_state->sensor_history);
					DEBUG("Found train %d at sensor %s, assuming %d errors" EOL, train_state->train_id, node->name, errors_assumed);
					for (int m = 0; errors_assumed < errors_assumed_threshold && m < context.merges_hit_count; m++) {
						const struct track_edge *expected_branch_edge = context.merges_hit[m].branch_edge;
						const struct track_node *branch = expected_branch_edge->src;
						ASSERT(branch->type == NODE_BRANCH);

						const int distance_from_train = context.distance - context.merges_hit[m].distance;
						// TODO: this doesn't work correctly while the train is stopping, or under
						// acceleration, because train_eta_from_state doesn't do that
						const int estimated_time = last_sensor_hit_time + train_eta_from_state(state, train_state, distance_from_train);
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
							int last_modified = switch_historical_get_last_mod_time(&state->switch_history);
							DEBUG("Train estimated_time was %d, last_modified %d" EOL, estimated_time, last_modified);
							if (last_modified <= estimated_time) {
								switch_to_adjust = branch->num;
							}
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
						train_candidate_switch_to_adjust = switch_to_adjust;
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
		train_candidate,
		train_candidate_switch_to_adjust,
		train_candidate_distance,
	}
	*changed_switch = ;
	return train_candidate;
}

// TODO: this should probably also return the distance the train was thought to have travelled,
// so we can delurk next_sensor & next_sensor_mm & simplify the actual_speed calculation
struct attribution attribute_sensor_to_train(struct trainsrv_state *state, int sensor, int now) {
	const struct track_node *sensor_node = track_node_from_sensor(sensor);
	struct attribution attr = attribute_sensor_to_known_train(state, sensor_node, now);
	if (train == NULL && state->unknown_train_id > 0) {
		attr.train = get_train_state(state, state->unknown_train_id);
		attr.changed_switch = -1;
		attr.distance_travelled = 0;
	}
	return attr;
}
