#include <kernel.h>

#include "../user/track.h"
#include "../user/sys.h"
#include "../user/tracksrv.h"

void tracksrv_test_basic(void) {
	struct astar_node path[ASTAR_MAX_PATH];
	int l = astar_find_path(&track[0], &track[1], path);
	astar_print_path(path, l);
	int n = tracksrv_reserve_path(path, l, 1000);
	printf("Reserved %d segments of track"EOL, n);
	ASSERT(n > 0);
}

void tracksrv_tests_init(void) {
	init_tracka(track);

	start_servers();
	tracksrv_start();

	tracksrv_test_basic();

	stop_servers();
}
