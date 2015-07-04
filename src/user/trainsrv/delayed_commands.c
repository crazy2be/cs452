#include "delayed_commands.h"

#include "track_control.h"
#include "../sys.h"

struct reverse_info {
	int train;
	int speed;
};
static void reverse_task(void) {
	int tid;
	struct reverse_info info;
	// our parent immediately sends us some bootstrap info
	receive(&tid, &info, sizeof(info));
	reply(tid, NULL, 0);

	tc_stop(info.train);
	delay(400);
	tc_toggle_reverse(info.train);
	tc_set_speed(info.train, info.speed);
}
void start_reverse(int train, int speed) {
	int tid = create(HIGHER(PRIORITY_MIN, 2), reverse_task);
	struct reverse_info info = (struct reverse_info) {
		.train = train,
		 .speed = speed,
	};
	send(tid, &info, sizeof(info), NULL, 0);
}
