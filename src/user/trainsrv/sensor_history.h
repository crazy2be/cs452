#pragma once

#undef HISTORY_LEN
#undef HISTORY_VAL
#undef HISTORY_PREFIX
#define HISTORY_LEN 20 // chosen arbitrarily
#define HISTORY_VAL int
#define HISTORY_PREFIX sensor_historical
#include "../history.h"
