#include "train_alert_srv.h"
#include "trainsrv_internal.h"
#include "../sys.h"
#include "../track.h"
#include "../buffer.h"
#include "../displaysrv.h"

// Other tasks can message this service, and request to be woken up
// when a train is at a particular point
//
// Generally, the way this works is that the trainserver sends us a message
// each time a train passes a sensor.
// If this is the last sensor that we would hit before arriving at a requested location,
// we begin counting down until the ETA of the train to that point.
//
// We need to be robust against a couple of conditions:
//
// Neither of these are implemented yet (the first is closer to being implemented)
//
// 1. The switches changing after we begin a "final approach". This will
//    redirect the train away from its predicted destination. We need to
//    cancel any wakeups that might have been started because we thought
//    we were on a final approach.
// 2. In similar fashion, we want to be resilient to changing the speed
//    of the train while it is on a final approach.

struct alert_request {
	struct position position;
	int train_id;
};

struct wakeup_call_data {
	struct alert_request_state *state;
	unsigned nonce;
	int ticks;
	bool actually_delay;
};

struct alertsrv_request {
	enum { ALERT, TRAIN_UPDATE, TRAIN_SPEED_UPDATE, SWITCH_UPDATE, WAKEUP } type;
	union {
		struct alert_request alert;
		struct {
			int train_id;
			struct position position;
		} train_update;
		struct {
			int train_id;
		} train_speed_update;
		struct switch_state switch_update;
		struct wakeup_call_data wakeup;
	} u;
};

struct alert_request_state {
	// the original alert request
	struct alert_request request;

	enum { UNUSED, WAITING, FINAL_APPROACH } state;
	int tid;

	// Designed to be robust against switches changing after
	// we enter our final approach
	// We maintain a list of switches that could, if switched, direct the train
	// away from its intended destination.
	// When one of these switches is switched while the train is in its final approach,
	// we increment the nonce.
	// Whenever we prepare a wakeup call, we tell it the nonce the request currently has.
	// When the wakeup call is made, if the nonce has changed since, we ignore the wakeup,
	// since it means that the switches changed, and the wakeup call was based on
	// data which has since changed.
	struct switch_state disrupting_switches;
	unsigned nonce;

	struct alert_request_state *next_state;
};

#define MAX_ACTIVE_REQUESTS 30

static void request_wakeup_call(struct alert_request_state *state, int time, bool actually_delay) {

	ASSERT(state->state == WAITING);
	state->state = FINAL_APPROACH;

	struct alertsrv_request req;
	req.type = WAKEUP;
	req.u.wakeup.state = state;
	req.u.wakeup.nonce = state->nonce;
	req.u.wakeup.actually_delay = actually_delay;

	delay_async(time, &req, sizeof(req), offsetof(struct alertsrv_request, u.wakeup.ticks));
}

struct final_approach_ctx {
	const struct track_edge *target_edge;
	int distance;
	struct switch_state disrupting_switches;
	bool success;
};

static bool break_for_final_approach(const struct track_edge *edge, void *context_) {
	struct final_approach_ctx *context = (struct final_approach_ctx*) context_;
	if (edge->src->type == NODE_BRANCH) {
		switch_set(&context->disrupting_switches, edge->src->num, 1);
	}
	if (edge == context->target_edge) {
		context->success = true;
		return 1;
	}
	context->distance += edge->dist;
	return edge->dest->type == NODE_SENSOR;
}

static void check_if_train_on_final_approach(struct alert_request_state *state,
        const struct position *current_position, const struct switch_state *switches,
        bool actually_delay) {
	if (position_is_uninitialized(current_position)) return;
	const struct position *target_position = &state->request.position;

	// special case to handle us having *just* passed that point
	if (current_position->edge == target_position->edge
	        && current_position->displacement > target_position->displacement) {
		logf("We just passed that point");
		return;
	}

	struct final_approach_ctx context = {};
	context.target_edge = target_position->edge;

	track_go_forwards(current_position->edge->src, switches, break_for_final_approach, &context);
	if (!context.success) {
		//logf("Not on final approach");
		return;
	}
	char current_repr[20], target_repr[20];

	position_repr(*current_position, current_repr);
	position_repr(*target_position, target_repr);
	logf("Train %d (%s) on final approach to %s",
			state->request.train_id, current_repr, target_repr);

	// find distance until we hit the target position, accounting for displacements
	// of the source & target positions
	const int distance_left = context.distance - current_position->displacement
	                          + target_position->displacement;

	const int arrival_time = trains_query_arrival_time(state->request.train_id, distance_left);

	request_wakeup_call(state, arrival_time, actually_delay);
}

static void handle_train_update(int train_id, struct position *position,
                                struct alert_request_state **states_for_train, const struct switch_state *switches,
                                bool actually_delay) {
	struct alert_request_state *state = states_for_train[train_id - 1];
	while (state) {
		check_if_train_on_final_approach(state, position, switches, actually_delay);
		state = state->next_state;
	}
}

static void handle_alert_request(struct alert_request_state *state,
                                 const struct switch_state *switches, bool actually_delay) {

	state->nonce = 0;
	state->state = WAITING;

	struct train_state train_state;
	trains_query_spatials(state->request.train_id, &train_state);
	check_if_train_on_final_approach(state, &train_state.position, switches, actually_delay);
}

static void handle_wakeup_call(struct wakeup_call_data *data,
                               struct alert_request_state **states_for_train, struct alert_request_state **freelist) {

	struct alert_request_state *state = data->state;

	// ignore wakeup calls that are outdated
	if (data->nonce != state->nonce) return;
	ASSERT(state->state == FINAL_APPROACH);

	state->state = UNUSED;

	// signal the awaiting task to tell it to wake up
	reply(state->tid, &data->ticks, sizeof(data->ticks));

	// remove the state from the list, and add it to the freelist
	struct alert_request_state **prev_state = &states_for_train[state->request.train_id - 1];
	while (*prev_state && *prev_state != state) prev_state = &(*prev_state)->next_state;

	// we should have found a state to remove
	ASSERT(*prev_state != NULL);

	*prev_state = state->next_state;
	state->next_state = *freelist;
	*freelist = state;
}

static void handle_train_speed_update(int train_id, struct alert_request_state **states_for_train) {
	struct alert_request_state *state = states_for_train[train_id - 1];
	while (state != NULL) {
		if (state->state == FINAL_APPROACH) {
			ASSERTF(0, "Updated speed for train %d while on a final approach", train_id);
		}
		state = state->next_state;
	}
}

static void handle_switch_update(const struct switch_state switches, const struct switch_state old_switches, struct alert_request_state **states_for_train) {
	for (int i = 0; i < NUM_TRAIN; i++) {
		struct alert_request_state *state = states_for_train[i];
		while (state != NULL) {
			// do a bit of poking at the internals since I don't want to write
			// a fancy interface for something I don't know if I'll do again
			// If this pattern emerges elsewhere, revisit
			if (state->state == FINAL_APPROACH && ((switches.packed ^ old_switches.packed) & state->disrupting_switches.packed)) {
				ASSERTF(0, "Switches were updated under us while train %d was on final approach", state->request.train_id);
			}
			state = state->next_state;
		}
	}
}

static void train_server_run(struct switch_state switches, bool actually_delay) {
	struct alert_request_state states[MAX_ACTIVE_REQUESTS];
	struct alert_request_state *states_for_train[NUM_TRAIN];
	struct alert_request_state *freelist;

	memset(&states, 0, sizeof(states));
	memset(&states_for_train, 0, sizeof(states_for_train));
	freelist = &states[0];
	for (int i = 1; i < MAX_ACTIVE_REQUESTS; i++) {
		states[i - 1].next_state = &states[i];
	}

	for (;;) {
		int tid;
		struct alertsrv_request req;
		receive(&tid, &req, sizeof(req));

		switch (req.type) {
		case ALERT: {
			ASSERTF(freelist != NULL, "Alert srv couldn't handle more requests");

			struct alert_request_state *state = freelist;
			state->tid = tid;
			state->request = req.u.alert;

			freelist = freelist->next_state;
			int offset = req.u.alert.train_id - 1;
			state->next_state = states_for_train[offset];
			states_for_train[offset] = state;

			handle_alert_request(state, &switches, actually_delay);
			break;
		}
		case TRAIN_UPDATE:
			reply(tid, NULL, 0);
			handle_train_update(req.u.train_update.train_id, &req.u.train_update.position,
			                    states_for_train, &switches, actually_delay);
			break;
		case TRAIN_SPEED_UPDATE:
			reply(tid, NULL, 0);
			handle_train_speed_update(req.u.train_speed_update.train_id, states_for_train);
			break;
		case SWITCH_UPDATE:
			reply(tid, NULL, 0);
			handle_switch_update(req.u.switch_update, switches, states_for_train);
			switches = req.u.switch_update;
			break;
		case WAKEUP:
			reply(tid, NULL, 0);
			handle_wakeup_call(&req.u.wakeup, states_for_train, &freelist);
			break;
		default:
			ASSERTF(0, "Unknown train alert srv request type %d", req.type);
			break;
		}
	}
}

#define TRAIN_ALERT_SRV_NAME "tasrv"

struct train_alert_params {
	struct switch_state switches;
	bool actually_delay;
};

static void train_alert_server(void) {
	struct train_alert_params params;
	int tid;
	ASSERT(receive(&tid, &params, sizeof(params)) == sizeof(params));
	register_as(TRAIN_ALERT_SRV_NAME);
	reply(tid, NULL, 0);

	train_server_run(params.switches, params.actually_delay);
}


void train_alert_start(struct switch_state switches, bool actually_delay) {
	// one higher than train server
	int tid = create(PRIORITY_TRAIN_ALERT_SRV, train_alert_server);
	struct train_alert_params params = { switches, actually_delay };
	send(tid, &params, sizeof(params), NULL, 0);
}

static int train_alert_tid(void) {
	static int tid_memo = -1;
	if (tid_memo < 0) {
		tid_memo = whois(TRAIN_ALERT_SRV_NAME);
	}
	return tid_memo;
}

void train_alert_update_train(int train_id, struct position position) {
	struct alertsrv_request req;
	req.type = TRAIN_UPDATE;
	req.u.train_update.train_id = train_id;
	req.u.train_update.position = position;
	send_async(train_alert_tid(), &req, sizeof(req));
}

void train_alert_update_train_speed(int train_id) {
	struct alertsrv_request req;
	req.type = TRAIN_SPEED_UPDATE;
	req.u.train_speed_update.train_id = train_id;
	send_async(train_alert_tid(), &req, sizeof(req));
}

void train_alert_update_switch(struct switch_state switches) {
	struct alertsrv_request req;
	req.type = SWITCH_UPDATE;
	memcpy(&req.u.switch_update, &switches, sizeof(req.u.switch_update));
	send_async(train_alert_tid(), &req, sizeof(req));
}

int train_alert_at(int train_id, struct position position) {
	struct alertsrv_request req;
	int resp;
	req.type = ALERT;
	req.u.alert.train_id = train_id;
	req.u.alert.position = position;
	send(train_alert_tid(), &req, sizeof(req), &resp, sizeof(resp));
	return resp;
}
