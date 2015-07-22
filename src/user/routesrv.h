#pragma once

#include <astar.h>

void routesrv_start(void);

// Blocking call
int routesrv_plan(struct track_node *start, struct track_node *end,
	struct astar_node (*path_out)[ASTAR_MAX_PATH]);
