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

HISTORY_KVP HISTORY_GET_KVP_BY_INDEX(const HISTORY_T *s, int index) {
	ASSERT(index > 0);
	ASSERTF(s->len >= index, "%d = index > len = %d in %s", index, s->len, __func__);
	return s->history[NORMALIZE_OFFSET(s->offset, -index)];
}

HISTORY_KVP HISTORY_GET_KVP_CURRENT(const HISTORY_T *s) {
	ASSERTF(s->len >= 1, "%d = index > len = %d in %s", 1, s->len, __func__);
	return HISTORY_GET_KVP_BY_INDEX(s, 1);
}

// return the most recent switch state that was set before the provided time
HISTORY_KVP HISTORY_GET_KVP_WITH_INDEX(const HISTORY_T *s, int time, int *index) {
	ASSERT(s->len > 0);
	int prev_index = NORMALIZE_OFFSET(s->offset, -1);
	int i = 1;
	while (i <= s->len) {
		int index = NORMALIZE_OFFSET(s->offset, - (i + 1));
		ASSERT(s->history[index].time < s->history[prev_index].time);
		if (s->history[index].time < time) break;
		prev_index = index;
		i++;
	}
	if (index) *index = i;
	return s->history[prev_index];
}

void HISTORY_SET(HISTORY_T *s, HISTORY_VAL current, int time) {
	int last = NORMALIZE_OFFSET(s->offset, -1);
	ASSERTF(s->len == 0 || time >= s->history[last].time,
			"State records went backwards in time (%d >= %d), len = %d, for %s",
			time, s->history[last].time, s->len, __func__);
	// wipe current state if the time interval between states is empty
	if (s->len != 0 && s->history[last].time == time) {
		s->history[last].st = current;
	} else {
		s->history[s->offset].st = current;
		s->history[s->offset].time = time;

		s->offset = NORMALIZE_OFFSET(s->offset, 1);
		if (s->len != HISTORY_LEN) s->len++;
	}
}
