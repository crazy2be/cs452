#include <kernel.h>

#include "../user/track.h"
#include "../user/sys.h"
#include "../user/tracksrv.h"

void tracksrv_test_basic(void) {

}

void tracksrv_tests_init(void) {
	init_tracka(track);

	start_servers();
	tracksrv_start();

	tracksrv_test_basic();

	stop_servers();
}
