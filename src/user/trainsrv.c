#include "trainsrv.h"

#include "util.h"
#include "clockserver.h"
#include "nameserver.h"
#include "request_type.h"
#include "switch_state.h"
#include "displaysrv.h"
#include "calibrate/calibrate.h"

#include <kernel.h>
#include <assert.h>

struct trains_request {
	enum request_type type;

	int train_number;
	int speed;

	int switch_number;
	enum sw_direction direction; // TODO: union?

	int distance;
	struct sensor_state sensors;
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
	ASSERT((1 <= sw && sw <= 18) || (145 <= sw && sw <= 148) || (150 <= sw && sw <= 156));
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
	// TODO: displaysrv_* functions should do this automatically?
	/* int displaysrv = whois(DISPLAYSRV_NAME); */
	static int train_speeds[NUM_TRAIN] = {};
	struct switch_state switches = {};
	for (int i = 1; i <= 18; i++) {
		tc_switch_switch(i, CURVED);
		switch_set(&switches, i, CURVED);
		// Avoid flooding rbuf. We probably shouldn't need this, since rbuf
		// should have plenty of space, but it seems to work...
		delay(1);
	}
	tc_switch_switch(153, CURVED);
	switch_set(&switches, 153, CURVED);
	tc_switch_switch(156, CURVED);
	switch_set(&switches, 156, CURVED);
	tc_deactivate_switch();
	/* displaysrv_update_switch(displaysrv, &switches); */

	calibrate_send_switches(whois(CALIBRATESRV_NAME), &switches);

	for (;;) {
		int tid = -1;
		struct trains_request req;
		receive(&tid, &req, sizeof(req));

		/* printf("Trains server got message! %d"EOL, req.type); */
		switch (req.type) {
		case SET_SPEED:
			// TODO: What do we do if we are already reversing or something?
			tc_set_speed(req.train_number, req.speed);
			train_speeds[req.train_number - MIN_TRAIN] = req.speed;
			reply(tid, NULL, 0);
			break;
		case REVERSE:
			start_reverse(req.train_number, train_speeds[req.train_number - MIN_TRAIN]);
			reply(tid, NULL, 0);
			break;
		case SWITCH_SWITCH:
			start_switch(req.switch_number, req.direction);
			switch_set(&switches, req.switch_number, req.direction);
			/* displaysrv_update_switch(displaysrv, &switches); */
			reply(tid, NULL, 0);
			break;
		default:
			WTF("UNKNOWN TRAINS REQ %d"EOL, req.type);
			break;
		}
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

static void trains_send(struct trains_request req, void *rpy, int rpyl) {
	ASSERTOK(send(trains_tid(), &req, sizeof(req), rpy, rpyl));
}
#define TSEND(req) trains_send(req, NULL, 0)
#define TSEND2(req, rpy) trains_send(req, rpy, sizeof(*(rpy)))

void trains_query_spatials(int train, struct train_state *state_out) {
	TSEND2(((struct trains_request) {
		.type = QUERY_SPATIALS,
		.train_number = train,
	}), state_out);
}

int trains_query_arrival_time(int train, int distance) {
	int rpy = -1;
	TSEND2(((struct trains_request) {
		.type = QUERY_ARRIVAL,
		.train_number = train,
		.distance = distance,
	}), &rpy);
	return rpy;
}

void trains_send_sensors(struct sensor_state state) {
	TSEND(((struct trains_request) {
		.type = SEND_SENSORS,
		.sensors = state,
	}));
}

void trains_set_speed(int train, int speed) {
	TSEND(((struct trains_request) {
		.type = SET_SPEED,
		.train_number = train,
		.speed = speed,
	}));
}

void trains_reverse(int train) {
	TSEND(((struct trains_request) {
		.type = REVERSE,
		.train_number = train,
	}));
}

void trains_switch(int switch_number, enum sw_direction d) {
	TSEND(((struct trains_request) {
		.type = SWITCH_SWITCH,
		.switch_number = switch_number,
		.direction = d,
	}));
}
