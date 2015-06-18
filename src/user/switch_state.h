#pragma once

#include <assert.h>

// switches are packed so it's cheap to message pass this struct around
struct switch_state {
	unsigned packed;
};

// TODO: later, we may want to add left/right distinction, to
// simplify dealing with the tri-state switches on the track
enum sw_direction { STRAIGHT, CURVED };

#define SWITCH_COUNT 22

static inline int switch_packed_num(int num) {
	if (1 <= num && num <= 18) {
		return num;
	} else if (153 <= num && num <= 156) {
		return num - 153 + 18 + 1;
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
