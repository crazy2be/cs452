#include <kernel.h>

#include "../user/track.h"
#include "../user/sys.h"
#include "../user/tracksrv.h"

void tracksrv_test_basic(void) {
	struct astar_node path[ASTAR_MAX_PATH];
	bool blocked_table[TRACK_MAX] = {};
	int l = astar_find_path(&track[0], &track[1], path, blocked_table);
	astar_print_path(path, l);
	int n = tracksrv_reserve_path_test(path, l, 1000, 1);
	printf("Reserved %d segments of track"EOL, n);
	ASSERT(n > 0);
	int n2 = tracksrv_reserve_path_test(path, l, 1000, 1);
	printf("Reserved %d segments of track"EOL, n2);
	ASSERT(n2 > 0);
	int n3 = tracksrv_reserve_path_test(path, l, 1000, 2);
	printf("Reserved %d segments of track"EOL, n3);
	ASSERT(n3 < 0);
}

void tracksrv_tests_init(void) {
	init_tracka(track);

	start_servers();
	tracksrv_start();

	tracksrv_test_basic();

	stop_servers();
}
