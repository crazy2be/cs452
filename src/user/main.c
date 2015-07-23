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
	create(PRIORITY_HEARTBEAT_DEBUG, heartbeat);
};

void init(void) {
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

	stop_servers();
}

int main(int argc, char *argv[]) {
	boot(init, PRIORITY_MIN, 1);
}
