#pragma once

#include <util.h>

#include "calibrate/track_data_new.h"
#include "switch_state.h"

extern struct track_node track[TRACK_MAX];

const struct track_node *track_node_from_sensor(int sensor);
const struct track_node *track_next_sensor(const struct track_node *current, const struct switch_state *switches, int *distance_out);
typedef bool (*break_cond)(const struct track_edge *e, void *ctx);
const struct track_node *track_go_forwards(const struct track_node *cur,
        const struct switch_state *sw, break_cond cb, void *ctx);
