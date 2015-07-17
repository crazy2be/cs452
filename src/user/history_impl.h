#define NORMALIZE_OFFSET PASTER(HISTORY_PREFIX, NORMALIZE_OFFSET)

#include <assert.h>

void HISTORY_INIT(HISTORY_T *s) {
	s->offset = 0;
	s->len = 0;
}

static inline int NORMALIZE_OFFSET(int initial, int delta) {
	initial += delta;
	// these conditions should get optimized out during inlining
	if (delta < 0) {
		if (initial < 0) initial = HISTORY_LEN - 1;
	} else if (delta >= 0) {
		initial %= HISTORY_LEN;
	}
	return initial;
}

HISTORY_VAL HISTORY_GET_BY_INDEX(const HISTORY_T *s, int index) {
	ASSERT(s->len > index);
	return s->history[NORMALIZE_OFFSET(s->offset, -index)].st;
}

HISTORY_VAL HISTORY_GET_CURRENT(const HISTORY_T *s) {
	return HISTORY_GET_BY_INDEX(s, 1);
}

int HISTORY_GET_LAST_MOD_TIME(const HISTORY_T *s) {
	return HISTORY_GET_MOD_BY_INDEX(s, 1);
}

int HISTORY_GET_MOD_BY_INDEX(const HISTORY_T *s, int index) {
	ASSERT(s->len > index);
	return s->history[NORMALIZE_OFFSET(s->offset, -index)].time;
}

// return the most recent switch state that was set before the provided time
HISTORY_VAL HISTORY_GET(const HISTORY_T *s, int time) {
	ASSERT(s->len > 0);
	int prev_index = NORMALIZE_OFFSET(s->offset, -1);

	for (int i = 2; i <= s->len; i++) {
		int index = NORMALIZE_OFFSET(s->offset, - i);
		ASSERT(s->history[index].time < s->history[prev_index].time);
		if (s->history[index].time < time) return s->history[prev_index].st;
		prev_index = index;
	}
	/* ASSERT(s->history[prev_index].time <= time); */
	return s->history[prev_index].st;
}

void HISTORY_SET(HISTORY_T *s, HISTORY_VAL current, int time) {
	int last = NORMALIZE_OFFSET(s->offset, -1);
	ASSERTF(s->len == 0 || time >= s->history[last].time, "Switch records went backwards in time");
	// wipe current state if the time interval between states is empty
	if (s->history[last].time == time) {
		s->history[last].st = current;
	} else {
		s->history[s->offset].st = current;
		s->history[s->offset].time = time;

		s->offset = NORMALIZE_OFFSET(s->offset, 1);
		if (s->len != HISTORY_LEN) s->len++;
	}
}
