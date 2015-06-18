#pragma once

// switches are packed so it's cheap to message pass this struct around
struct switch_state {
	unsigned packed;
};

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

static inline void switch_set(struct switch_state *s, int num, int curved) {
	const int bit = 0x1 << switch_packed_num(num);
	s->packed = curved ? (s->packed | bit) : (s->packed & ~bit);
}

static inline int switch_get(const struct switch_state *s, int num) {
	const int bit = 0x1 << switch_packed_num(num);
	return s->packed & bit;
}
