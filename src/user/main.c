#include <kernel.h>
#include <assert.h>
#include "sys.h"
#include "signal.h"
#include <util.h>
#include "commandsrv.h"
#include "displaysrv.h"
#include "sensorsrv.h"
#include "trainsrv.h"
#include "routesrv.h"
#include "calibrate.h"
#include "track.h"

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
	start_servers();

	for (int i = 0; i < 4; i++) {
		create(LOWER(PRIORITY_MAX, i + 3), client_task);
	}
	struct init_reply rpys[4] = {{10, 20}, {23, 9}, {33, 6}, {71, 3}};
	int tids[4] = {};
	for (int i = 0; i < 4; i++) receive(&tids[i], NULL, 0);
	for (int i = 0; i < 4; i++) reply(tids[i], &rpys[i], sizeof(rpys[i]));

	for (int i = 0; i < 4; i++) signal_recv();

	stop_servers();
}

static int whois_poll(const char *name) {
	int tid;
	while ((tid = try_whois(name)) < 0) {
		delay(1);
	}
	return tid;
}

static void heartbeat(void) {
	int count = 0;
	int trains_tid = whois_poll("trains");
	int display_tid = whois_poll("displaysrv");
	int command_tid = whois_poll("commandsrv");
	for (;;) {
		delay(10);
		struct task_info trains_info, display_info, command_info;
		task_status(trains_tid, &trains_info);
		task_status(display_tid, &display_info);
		task_status(command_tid, &command_info);

		printf("\e[s\e[5;90H%d"
			   "\e[6;90Htrainsrv status = %d"
			   "\e[7;90Hdisplaysrv status = %d"
			   "\e[8;90Hcommandsrv status = %d"
			   "\e[u", count++, trains_info.state, display_info.state, command_info.state);
	}
}

static void heartbeat_start(void) {
	create(LOWER(PRIORITY_MAX, 4), heartbeat);
};

void test_init(void) {
	// initialize the track

#ifdef TRACKA
	init_tracka(track);
#else
	init_trackb(track);
#endif

	start_servers();

#if CALIBRATE
	calibratesrv();
#else
	displaysrv();
	commandsrv();
	trains_start();
	routesrv_start();
	heartbeat_start();
#endif
	sensorsrv();

	/* printf("Hello world" EOL); */
	/* char buf[] = {0xE2, 0x94, 0x90}; */
	/* char buf[128]; */
	/* for (int i = 0; i < 128; i++) { */
	/* 	buf[i] = 128 + i; */
	/* } */
	/* fput_buf(COM2, buf, sizeof(buf)); */
	/* fput_buf(COM2, buf, sizeof(buf)); */
	/* fput_buf(COM2, buf, sizeof(buf)); */
	/* fput_buf(COM2, buf, sizeof(buf)); */
	/* printf("Hello world" EOL); */

	/* int switches[] = {4, 12}; */

	/* for (int i = 0; i < sizeof(switches) / sizeof(switches[0]); i++) { */
	/* 	set_switch_state(switches[i], STRAIGHT); */
	/* 	if (i % 8 == 7) { */
	/* 		delay(10); */
	/* 		disable_switch_solenoid(); */
	/* 	} */
	/* } */
	/* delay(10); */
	/* disable_switch_solenoid(); */

	/* printf("Goodbye, world" EOL); */

	/* delay(100); */
	/* stop_servers(); */
}

#include "benchmark.h"
int main(int argc, char *argv[]) {
	boot(test_init, PRIORITY_MIN, 1);
}
