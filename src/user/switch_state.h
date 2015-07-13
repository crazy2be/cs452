#pragma once

#include <assert.h>

// switches are packed so it's cheap to message pass this struct around
struct switch_state {
	unsigned packed;
};

#define SWITCH_HISTORY_LEN 20 // chosen arbitrarily
struct switch_historical_state {
	// this is a ringbuffer which overwrites itself in a loop
	// elements within the ringbuffer are ordered by time, with
	// the most recent at the start of the buffer
	struct {
		struct switch_state st;
		int time;
	} history[SWITCH_HISTORY_LEN];

	// points to the next element to be written to (so most recently written
	// element is at a position one less)
	int offset;
	int len;
};

// TODO: later, we may want to add left/right distinction, to
// simplify dealing with the tri-state switches on the track
enum sw_direction { STRAIGHT, CURVED };

#define SWITCH_COUNT ((18 - 1 + 1) + (156 - 145 + 1))

static inline int switch_packed_num(int num) {
	if (1 <= num && num <= 18) {
		return num;
	} else if (145 <= num && num <= 156) {
		return num - 145 + 18 + 1;
	} else {
		ASSERT(0 && "Unknown switch number");
		return -1;
	}
}

static inline void switch_set(struct switch_state *s, int num, enum sw_direction d) {
	const int bit = 0x1 << switch_packed_num(num);
	s->packed = (d == CURVED) ? (s->packed | bit) : (s->packed & ~bit);
}

static inline enum sw_direction switch_get(const struct switch_state *s, int num) {
	const int bit = 0x1 << switch_packed_num(num);
	return (s->packed & bit) ? CURVED : STRAIGHT;
}

void switch_historical_init(struct switch_historical_state *s);
struct switch_state switch_historical_get_current(const struct switch_historical_state *s);
struct switch_state switch_historical_get(const struct switch_historical_state *s, int time);
void switch_historical_set(struct switch_historical_state *s, struct switch_state current, int time);
