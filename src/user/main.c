#include <kernel.h>
#include <io.h>
#include <assert.h>
#include "nameserver.h"
#include "clockserver.h"
#include "io_server.h"
#include "rps.h"
#include "signal.h"
#include <util.h>
#include "track_control.h"
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

void test_init(void) {
	create(LOWER(PRIORITY_MAX, 3), nameserver);
	ioserver(LOWER(PRIORITY_MAX, 2), COM1);
	create(LOWER(PRIORITY_MAX, 1), clockserver);

	/* set_train_speed(45, 15); */
	/* set_switch_state(4, STRAIGHT); */
	set_switch_state(12, STRAIGHT);
	delay(10);
	disable_switch_solenoid();
	delay(10);

	/* set_switch_state(4, CURVED); */
	set_switch_state(12, CURVED);
	delay(10);
	disable_switch_solenoid();
	delay(10);
}

#include "benchmark.h"
int main(int argc, char *argv[]) {
	boot(test_init, PRIORITY_MIN, 1);
}
