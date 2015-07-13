#include "switch_state.h"

void switch_historical_init(struct switch_historical_state *s) {
	s->offset = 0;
	s->len = 0;
}

struct switch_state switch_historical_get_current(const struct switch_historical_state *s) {
	ASSERT(s->len > 0);
	return s->history[s->offset].st;
}

static inline int normalize_offset(int initial, int delta) {
	initial += delta;
	// these conditions should get optimized out during inlining
	if (delta < 0) {
		if (initial < 0) initial = SWITCH_HISTORY_LEN - 1;
	} else if (delta >= 0) {
		initial %= SWITCH_HISTORY_LEN;
	}
	return initial;
}

// return the most recent switch state that was set before the provided time
struct switch_state switch_historical_get(const struct switch_historical_state *s, int time) {
	ASSERT(s->len > 0);
	int prev_index = normalize_offset(s->offset, -1);

	for (int i = 2; i <= s->len; i++) {
		int index = normalize_offset(s->offset, - i);
		if (s->history[index].time < time) return s->history[prev_index].st;
		prev_index = index;
	}
	/* ASSERT(s->history[prev_index].time <= time); */
	return s->history[prev_index].st;
}

void switch_historical_set(struct switch_historical_state *s, struct switch_state current, int time) {
	s->history[s->offset].st = current;
	s->history[s->offset].time = time;

	s->offset = normalize_offset(s->offset, 1);
	if (s->len != SWITCH_HISTORY_LEN) s->len++;
}
