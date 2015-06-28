#include "delayed_commands.h"

#include "track_control.h"
#include "../clockserver.h"

struct reverse_info { int train; int speed; };
static void reverse_task(void) {
	int tid;
	struct reverse_info info;
	// our parent immediately try_sends us some bootstrap info
	try_receive(&tid, &info, sizeof(info));
	try_reply(tid, NULL, 0);

	tc_stop(info.train);
	delay(400);
	tc_toggle_reverse(info.train);
	tc_set_speed(info.train, info.speed);
}
void start_reverse(int train, int speed) {
	int tid = try_create(HIGHER(PRIORITY_MIN, 2), reverse_task);
	struct reverse_info info = (struct reverse_info) {
		.train = train,
		.speed = speed,
	};
	try_send(tid, &info, sizeof(info), NULL, 0);
}

struct switch_info { int sw; enum sw_direction d; };
static void switch_task(void) {
	int tid;
	struct switch_info info;
	// our parent immediately try_sends us some bootstrap info
	try_receive(&tid, &info, sizeof(info));
	try_reply(tid, NULL, 0);

	tc_switch_switch(info.sw, info.d);
	delay(100);
	tc_deactivate_switch();
}
void start_switch(int sw, enum sw_direction d) {
	int tid = try_create(HIGHER(PRIORITY_MIN, 2), switch_task);
	struct switch_info info = (struct switch_info) {
		.sw = sw,
		.d = d,
	};
	try_send(tid, &info, sizeof(info), NULL, 0);
}
