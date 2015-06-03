#include <kernel.h>
#include <io.h>
#include <assert.h>
#include "nameserver.h"
#include "clockserver.h"
#include "../kernel/util.h"
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
		printf("tid: %d, interval: %d, delay: %d\n", tid(), rpy.delay_time, i);
	}
}

void init(void) {
	create(LOWER(PRIORITY_MAX, 2), nameserver);
	create(LOWER(PRIORITY_MAX, 1), clockserver);

	for (int i = 0; i < 4; i++) {
		create(LOWER(PRIORITY_MAX, i + 3), client_task);
	}
	struct init_reply rpys[4] = {{10, 20}, {23, 9}, {33, 6}, {71, 3}};
	for (int i = 0; i < 4; i++) {
		int tid = -1;
		receive(&tid, NULL, 0);
		reply(tid, &rpys[i], sizeof(rpys[i]));
	}
}

#include "benchmark.h"
int main(int argc, char *argv[]) {
	boot(init, 0);
	/* boot(await_init_task, 0); */
	/* boot(init_task, 0); */
}
