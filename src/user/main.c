#include <kernel.h>
#include <io.h>
#include <assert.h>
#include "nameserver.h"
#include "clockserver.h"
#include "rps.h"
#include "signal.h"
#include <util.h>
#include "../kernel/drivers/timer.h"

struct init_reply {
	int delay_time;
	int delay_count;
};

void client_task(void) {
	struct init_reply rpy;
	send(1, NULL, 0, &rpy, sizeof(rpy));
	for (int i = 0; i < rpy.delay_count; i++) {
		delay(rpy.delay_time);
		printf("tid: %d, interval: %d, round: %d" EOL, tid(), rpy.delay_time, i);
	}
	signal_send(parent_tid());
}

void init(void) {
	create(LOWER(PRIORITY_MAX, 2), nameserver);
	create(LOWER(PRIORITY_MAX, 1), clockserver);

	for (int i = 0; i < 4; i++) {
		create(LOWER(PRIORITY_MAX, i + 3), client_task);
	}
	struct init_reply rpys[4] = {{10, 20}, {23, 9}, {33, 6}, {71, 3}};
	int tids[4] = {};
	for (int i = 0; i < 4; i++) receive(&tids[i], NULL, 0);
	for (int i = 0; i < 4; i++) reply(tids[i], &rpys[i], sizeof(rpys[i]));

	for (int i = 0; i < 4; i++) signal_recv();
	shutdown_clockserver();
}

#include "benchmark.h"
int main(int argc, char *argv[]) {
	boot(init, 0, 1);
// 	extern void rps_init_task(void);
// 	boot(rps_init_task, 0);
	/* boot(await_init_task, 0); */
	/* boot(init_task, 0); */
}