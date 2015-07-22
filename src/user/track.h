#pragma once

#include <util.h>

#include "trainsrv/track_node.h"
#include "trainsrv/track_data.h"
#include "switch_state.h"

extern struct track_node track[TRACK_MAX];
#define TRACK_NODE_INDEX(node) (node - track)

const struct track_node *track_node_from_sensor(int sensor);
const struct track_node *track_next_sensor(const struct track_node *current, const struct switch_state *switches, int *distance_out);
typedef bool (*break_cond)(const struct track_edge *e, void *ctx);
const struct track_node *track_go_forwards(const struct track_node *cur,
        const struct switch_state *sw, break_cond cb, void *ctx);
const struct track_node *track_go_forwards_cycle(const struct track_node *cur,
        const struct switch_state *sw, break_cond cb, void *ctx);
