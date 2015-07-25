#include "astar_test.h"

#include "../lib/astar.h"

void astar_tests(void) {
	init_tracka(track);
	{
		struct astar_node path[ASTAR_MAX_PATH];
		bool blocked_table[TRACK_MAX] = {};
		int l = astar_find_path(&track[0], &track[1], path, blocked_table);
		astar_print_path(path, l);
	}

	// b1 -> b13
	{
		struct astar_node path[ASTAR_MAX_PATH];
		int l = astar_find_path(&track[16], &track[28], path);
		astar_print_path(path, l);
	}
}
