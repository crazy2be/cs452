#include "delayed_commands.h"

#include "track_control.h"
#include "../trainsrv.h"
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

	trains_set_speed(info.train, 0);

	// allow enough time for the train to come to a full stop
	delay(400);
	trains_reverse_unsafe(info.train);

	// If we don't delay between sending the reverse command, and sending
	// the set speed command, the train doesn't always reverse
	// This is admittedly a hackjob, but it seems to work.
	delay(10);
	trains_set_speed(info.train, info.speed);
}
void start_reverse(int train, int speed) {
	int tid = create(HIGHER(PRIORITY_MIN, 2), reverse_task);
	struct reverse_info info = (struct reverse_info) {
		.train = train,
		 .speed = speed,
	};
	send(tid, &info, sizeof(info), NULL, 0);
}
