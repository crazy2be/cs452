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
#include "tracksrv.h"

void print_stacked_registers(int *sp) {
	int p = 0;
	int *pp = &p;
	kprintf("Here: %p"EOL, pp);
	for (int i = 0; i < 32; i++) {
		kprintf("+/-: %p %p"EOL, pp + i, pp - i);
		if (pp - i >= 0 && pp - i < (int*)0x2021800) kprintf("*+: %d %x"EOL, i, pp[i]);
		if (pp + i >= 0 && pp - i < (int*)0x2021800) kprintf("*-: %d %x"EOL, -i, pp[-i]);
	}
}

static int whois_poll(const char *name) {
	int tid;
	while ((tid = try_whois(name)) < 0) {
		delay(1);
	}
	return tid;
}

static void heartbeat(void) {
	//int count = 0;
	int trains_tid = whois_poll("trains");
	int display_tid = whois_poll("displaysrv");
	int command_tid = whois_poll("commandsrv");
	for (;;) {
		delay(10);
		struct task_info trains_info, display_info, command_info;
		task_status(trains_tid, &trains_info);
		task_status(display_tid, &display_info);
		task_status(command_tid, &command_info);

// 		printf("\e[s\e[1;100H%d"
// 			   "\e[1;110Hts %d"
// 			   "\e[1;120Hds %d"
// 			   "\e[1;130Hcs %d"
// 			   "\e[u", count++, trains_info.state, display_info.state, command_info.state);
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
	calibratesrv_start();
#else
	tracksrv_start();
	displaysrv_start();
	commandsrv_start();
	trains_start();
	routesrv_start();
	heartbeat_start();
#endif
	sensorsrv_start();
}

int main(int argc, char *argv[]) {
	boot(init, PRIORITY_MIN, 1);
}
