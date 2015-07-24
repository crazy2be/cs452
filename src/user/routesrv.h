#pragma once

#include <astar.h>

void routesrv_start(void);

// Utility for tests
void routesrv_blocked_table_from_reservation_table(int *reservation_table,
												   bool *blocked_table,
												   int caller_tid);

// Blocking call
int routesrv_plan(const struct track_node *start, const struct track_node *end,
	struct astar_node *path_out);

