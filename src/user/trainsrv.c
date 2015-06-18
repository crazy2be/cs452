#include "trainsrv.h"

#include "clockserver.h"
#include "nameserver.h"
#include "request_type.h"

#include <kernel.h>
#include <assert.h>

struct trains_request {
	enum request_type type;

	int train_number;
	int speed;

	int switch_number;
	enum sw_direction direction; // TODO: union?
};

#define MAX_TRAIN 80
#define MIN_TRAIN 1
#define NUM_TRAIN ((MAX_TRAIN) - (MIN_TRAIN) + 1)
static void tc_set_speed(int train, int speed) {
	ASSERT(1 <= train && train <= 80);
	ASSERT(0 <= speed && speed <= 14);
#ifdef QEMU
	ASSERT(fputs(COM1, "Changing train speed" EOL) == 0);
#else
	char train_command[2] = {speed, train};
	ASSERT(fput_buf(COM1, train_command, sizeof(train_command)) == 0);
#endif
}

static void tc_toggle_reverse(int train) {
	ASSERT(1 <= train && train <= 80);
#ifdef QEMU
	ASSERT(fputs(COM1, "Reversing train" EOL) == 0);
#else
	char train_command[2] = {15, train};
	ASSERT(fput_buf(COM1, train_command, sizeof(train_command)) == 0);
#endif
}

static void tc_stop(int train) {
	ASSERT(1 <= train && train <= 80);
#ifdef QEMU
	ASSERT(fputs(COM1, "Stopping train" EOL) == 0);
#else
	char train_command[2] = {0, train};
	ASSERT(fput_buf(COM1, train_command, sizeof(train_command)) == 0);
#endif
}

static void tc_switch_switch(int sw, enum sw_direction d) {
	ASSERT((1 <= sw && sw <= 18) || (153 <= sw && sw <= 156));
#ifdef QEMU
	ASSERT(fputs(COM1, "Changing switch position" EOL) == 0);
#else
	char cmd = (d == STRAIGHT) ? 0x21 : 0x22;
	char sw_command[2] = {cmd, sw};
	ASSERT(fput_buf(COM1, sw_command, sizeof(sw_command)) == 0);
#endif
}

static void tc_deactivate_switch(void) {
#ifdef QEMU
	ASSERT(fputs(COM1, "Resetting solenoid" EOL) == 0);
#else
	ASSERT(fputc(COM1, 0x20) == 0);
#endif
}

struct reverse_info { int train; int speed; };
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
static void start_reverse(int train, int speed) {
	int tid = create(HIGHER(PRIORITY_MIN, 2), reverse_task);
	struct reverse_info info = (struct reverse_info) {
		.train = train,
		.speed = speed,
	};
	send(tid, &info, sizeof(info), NULL, 0);
}

struct switch_info { int sw; enum sw_direction d; };
static void switch_task(void) {
	int tid;
	struct switch_info info;
	// our parent immediately sends us some bootstrap info
	receive(&tid, &info, sizeof(info));
	reply(tid, NULL, 0);

	tc_switch_switch(info.sw, info.d);
	delay(100);
	tc_deactivate_switch();
}
static void start_switch(int sw, enum sw_direction d) {
	int tid = create(HIGHER(PRIORITY_MIN, 2), switch_task);
	struct switch_info info = (struct switch_info) {
		.sw = sw,
		.d = d,
	};
	send(tid, &info, sizeof(info), NULL, 0);
}

static void trains_server(void) {
	register_as("trains");
	static int train_speeds[NUM_TRAIN] = {};
	//static int switch_states[NUM_SWITCH] = {}; // TODO

	for (;;) {
		int tid = -1, resp = -1;
		struct trains_request req;
		receive(&tid, &req, sizeof(req));

		/* printf("Trains server got message! %d"EOL, req.type); */
		switch (req.type) {
		case SET_SPEED:
			// TODO: What do we do if we are already reversing or something?
			tc_set_speed(req.train_number, req.speed);
			train_speeds[req.train_number - MIN_TRAIN] = req.speed;
			break;
		case REVERSE:
			start_reverse(req.train_number, train_speeds[req.train_number - MIN_TRAIN]);
			break;
		case SWITCH_SWITCH:
			start_switch(req.switch_number, req.direction);
			break;
		default:
			resp = -1;
			printf("UNKNOWN TRAINS REQ %d" EOL, req.type);
			break;
		}
		reply(tid, &resp, sizeof(resp));
	}
}

int start_trains(void) {
	return create(HIGHER(PRIORITY_MIN, 2), trains_server);
}

static int trains_tid(void) {
	static int ts_tid = -1;
	if (ts_tid < 0) {
		ts_tid = whois("trains");
	}
	return ts_tid;
}

static int trains_send(struct trains_request req) {
	int rpy;
	int l = send(trains_tid(), &req, sizeof(req), &rpy, sizeof(rpy));
	if (l != sizeof(rpy)) return l;
	return rpy;
}

int trains_set_speed(int train, int speed) {
	return trains_send((struct trains_request) {
		.type = SET_SPEED,
		.train_number = train,
		.speed = speed,
	});
}

int trains_reverse(int train) {
	return trains_send((struct trains_request) {
		.type = REVERSE,
		.train_number = train,
	});
}

int trains_switch(int switch_number, enum sw_direction d) {
	return trains_send((struct trains_request) {
		.type = SWITCH_SWITCH,
		.switch_number = switch_number,
		.direction = d,
	});
}
