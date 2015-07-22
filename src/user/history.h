#include <util.h>
#include <assert.h>

// this file should only be included by suorce files, not other headers
#ifndef HISTORY_LEN
#error "No HISTORY_LEN defined"
#endif

#ifndef HISTORY_VAL
#error "No HISTORY_VAL defined"
#endif

#ifndef HISTORY_PREFIX
#error "No HISTORY_PREFIX defined"
#endif

#define HISTORY_T struct PASTER(HISTORY_PREFIX, state)
#define HISTORY_KVP struct PASTER(HISTORY_PREFIX, kvp)
#define HISTORY_INIT PASTER(HISTORY_PREFIX, init)
#define HISTORY_GET_CURRENT PASTER(HISTORY_PREFIX, get_current)
#define HISTORY_GET_BY_INDEX PASTER(HISTORY_PREFIX, get_by_index)
#define HISTORY_GET PASTER(HISTORY_PREFIX, get)
#define HISTORY_GET_KVP_CURRENT PASTER(HISTORY_PREFIX, get_kvp_current)
#define HISTORY_GET_KVP_WITH_INDEX PASTER(HISTORY_PREFIX, get_kvp_with_index)
#define HISTORY_GET_KVP PASTER(HISTORY_PREFIX, get_kvp)
#define HISTORY_GET_KVP_BY_INDEX PASTER(HISTORY_PREFIX, get_kvp_by_index)
#define HISTORY_SET PASTER(HISTORY_PREFIX, set)

HISTORY_KVP {
	HISTORY_VAL st;
	int time;
};

HISTORY_T {
	// this is a ringbuffer which overwrites itself in a loop
	// elements within the ringbuffer are ordered by time, with
	// the most recent at the start of the buffer
	HISTORY_KVP history[HISTORY_LEN];

	// points to the next element to be written to (so most recently written
	// element is at a position one less)
	int offset;
	int len;
};

void HISTORY_INIT(HISTORY_T *s);

HISTORY_KVP HISTORY_GET_KVP_CURRENT(const HISTORY_T *s);
HISTORY_KVP HISTORY_GET_KVP_BY_INDEX(const HISTORY_T *s, int index);
// return the most recent switch state that was set before the provided time
HISTORY_KVP HISTORY_GET_KVP_WITH_INDEX(const HISTORY_T *s, int time, int *index);

static inline HISTORY_KVP HISTORY_GET_KVP(const HISTORY_T *s, int time) {
	return HISTORY_GET_KVP_WITH_INDEX(s, time, NULL);
}

void HISTORY_SET(HISTORY_T *s, HISTORY_VAL current, int time);

static inline HISTORY_VAL HISTORY_GET_BY_INDEX(const HISTORY_T *s, int index) {
	ASSERTF(s->len >= index, "%d = index > len = %d in %s", index, s->len, __func__);
	return HISTORY_GET_KVP_BY_INDEX(s, index).st;
}

static inline HISTORY_VAL HISTORY_GET_CURRENT(const HISTORY_T *s) {
	ASSERTF(s->len >= 1, "%d = index > len = %d in %s", 1, s->len, __func__);
	return HISTORY_GET_KVP_CURRENT(s).st;
}

static inline HISTORY_VAL HISTORY_GET(const HISTORY_T *s, int time) {
	return HISTORY_GET_KVP(s, time).st;
}
