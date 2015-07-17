#include <util.h>

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
#define HISTORY_INIT PASTER(HISTORY_PREFIX, init)
#define HISTORY_GET_CURRENT PASTER(HISTORY_PREFIX, get_current)
#define HISTORY_GET_BY_INDEX PASTER(HISTORY_PREFIX, get_by_index)
#define HISTORY_GET PASTER(HISTORY_PREFIX, get)
#define HISTORY_SET PASTER(HISTORY_PREFIX, set)
#define HISTORY_GET_LAST_MOD_TIME PASTER(HISTORY_PREFIX, get_last_mod_time)
#define HISTORY_GET_MOD_BY_INDEX PASTER(HISTORY_PREFIX, get_mod_by_index)

HISTORY_T {
	// this is a ringbuffer which overwrites itself in a loop
	// elements within the ringbuffer are ordered by time, with
	// the most recent at the start of the buffer
	struct {
		HISTORY_VAL st;
		int time;
	} history[HISTORY_LEN];

	// points to the next element to be written to (so most recently written
	// element is at a position one less)
	int offset;
	int len;
};

void HISTORY_INIT(HISTORY_T *s);
HISTORY_VAL HISTORY_GET_CURRENT(const HISTORY_T *s);
HISTORY_VAL HISTORY_GET_BY_INDEX(const HISTORY_T *s, int index);
int HISTORY_GET_LAST_MOD_TIME(const HISTORY_T *s);
int HISTORY_GET_MOD_BY_INDEX(const HISTORY_T *s, int index);
// return the most recent switch state that was set before the provided time
HISTORY_VAL HISTORY_GET(const HISTORY_T *s, int time);
void HISTORY_SET(HISTORY_T *s, HISTORY_VAL current, int time);
